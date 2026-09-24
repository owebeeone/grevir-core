"""Two-pass Arduino CLI build with a target-compiled Grevir interrupt probe."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

from protocol import PlanError
from verify import verify_ready


def _run(arguments: list[str], cwd: Path | None = None) -> None:
    subprocess.run(arguments, cwd=cwd, check=True)


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="grevir-irqgen arduino-build",
        description="Probe and build a staged sketch; see docs/guides/interrupts.md")
    for name in ("arduino-cli", "sketch", "work-dir", "output-dir", "fqbn",
                 "backend", "target", "board", "compiler", "application-header"):
        parser.add_argument(f"--{name}", required=True)
    parser.add_argument("--cpp-flags", default="-std=c++23",
                        help="C++ flags for both passes (default: -std=c++23)")
    parser.add_argument("--library", action="append", default=[])
    parser.add_argument("--build-property", action="append", default=[])
    args = parser.parse_args(arguments)
    source = Path(args.sketch).resolve()
    if not source.is_dir():
        raise PlanError("Arduino sketch directory is missing")
    app_header = Path(args.application_header)
    if app_header.is_absolute() or ".." in app_header.parts:
        raise PlanError("application header must be inside the staged sketch")
    if not (source / app_header).is_file():
        raise PlanError("application header is missing from the sketch")
    libraries = [str(Path(path).resolve()) for path in args.library]
    if any(value.startswith("compiler.cpp.extra_flags=")
           for value in args.build_property):
        raise PlanError("use --cpp-flags for compiler.cpp.extra_flags")
    output = Path(args.output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / "grevir_irq_firmware_ready.json").unlink(missing_ok=True)
    work = Path(args.work_dir).resolve() / uuid.uuid4().hex
    stage = work / source.name
    work.mkdir(parents=True)
    shutil.copytree(source, stage)
    probe = stage / "grevir_irq_probe.cpp"
    probe.write_text(f'#include "{app_header.as_posix()}"\n'
                     '#include <grevir/interrupt/probe_section.hpp>\n'
                     'GREVIR_EMIT_IRQ_PROBE(GrevirApplication);\n')
    base = [args.arduino_cli, "compile", "--fqbn", args.fqbn]
    for library in libraries:
        base += ["--library", library]
    for property_value in args.build_property:
        base += ["--build-property", property_value]
    probe_build = work / "probe-build"
    strict_build = work / "strict-build"
    bindings = work / "bindings"
    probe_flags = args.cpp_flags + " -DGREVIR_IRQ_PROBE=1"
    _run(base + ["--only-compilation-database", "--build-path", str(probe_build),
                 "--build-property", f"compiler.cpp.extra_flags={probe_flags}", str(stage)])
    database = json.loads((probe_build / "compile_commands.json").read_text())
    matches = [entry for entry in database
               if Path(entry["file"]).name == probe.name]
    if len(matches) != 1:
        raise PlanError("Arduino compilation database lacks one interrupt probe")
    entry = matches[0]
    if "arguments" not in entry:
        raise PlanError("Arduino compilation database lacks argument vector")
    # Arduino AVR uses LTO by default. A slim LTO object has no materialized
    # custom section, so only the probe compile disables LTO after its recipe.
    _run([*entry["arguments"], "-fno-lto"], Path(entry["directory"]))
    object_path = None
    for index, argument in enumerate(entry["arguments"][:-1]):
        if argument == "-o":
            object_path = Path(entry["arguments"][index + 1])
    if object_path is None or not object_path.is_file():
        raise PlanError("target probe object was not produced")
    attempt = uuid.uuid4().hex
    tool = Path(__file__).with_name("__main__.py")
    ready = bindings / "grevir_irq_ready.json"
    try:
        _run([sys.executable, "-B", str(tool), "plan", "--object", str(object_path),
              "--out-dir", str(bindings), "--attempt", attempt, "--backend",
              args.backend, "--target", args.target, "--board", args.board,
              "--compiler", args.compiler])
        _run([sys.executable, "-B", str(tool), "emit", "--out-dir", str(bindings),
              "--attempt", attempt, "--backend", args.backend, "--compiler",
              args.compiler, "--application-header", app_header.as_posix()])
        marker = verify_ready(bindings, args.backend, args.target, args.board,
                              args.compiler, attempt)
        probe.unlink()
        for extension in ("hpp", "cpp"):
            filename = f"grevir_generated_irq_bindings_{args.backend}.{extension}"
            shutil.copy2(bindings / filename, stage / filename)
        strict_flags = (args.cpp_flags + f" -I{stage}" + ' -DGREVIR_GENERATED_IRQ_HEADER="'
                        f'grevir_generated_irq_bindings_{args.backend}.hpp"')
        private_export = work / "firmware-export"
        _run(base + ["--build-path", str(strict_build), "--output-dir", str(private_export),
                     "--build-property", f"compiler.cpp.extra_flags={strict_flags}",
                     str(stage)])
        artifacts = {}
        for source_file in sorted(private_export.iterdir()):
            if source_file.is_file():
                content = source_file.read_bytes()
                temporary = output / f".{source_file.name}.{attempt}.tmp"
                temporary.write_bytes(content)
                os.replace(temporary, output / source_file.name)
                artifacts[source_file.name] = hashlib.sha256(content).hexdigest()
        if not artifacts:
            raise PlanError("Arduino build produced no firmware artifacts")
        firmware_ready = {"attempt": attempt, "binding_plan": marker["plan_fingerprint"],
                          "backend": args.backend, "target": args.target,
                          "board": args.board, "compiler": args.compiler,
                          "artifacts": artifacts}
        temporary = output / f".grevir_irq_firmware_ready.{attempt}.tmp"
        temporary.write_text(json.dumps(firmware_ready, sort_keys=True, indent=2) + "\n")
        os.replace(temporary, output / "grevir_irq_firmware_ready.json")
    except (OSError, subprocess.CalledProcessError, PlanError):
        ready.unlink(missing_ok=True)
        raise
    print(f"grevir-irqgen: Arduino build ready: {output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.CalledProcessError, PlanError) as error:
        print(f"grevir-irqgen: {error}", file=sys.stderr)
        raise SystemExit(1)
