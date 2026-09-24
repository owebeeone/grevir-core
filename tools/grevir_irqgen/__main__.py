"""grevir-irqgen: probe object to canonical JSON and target binding files."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

from emit import EMITTER, emit, output_identity
from object_section import ObjectSectionError, extract_interrupt_plan
from protocol import PlanError, canonical_bytes, decode, parse_canonical
from arduino_build import main as arduino_build_main


def _write(path: Path, content: bytes) -> None:
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    try:
        temporary.write_bytes(content)
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def _fail(stage: str) -> None:
    if os.environ.get("GREVIR_IRQGEN_FAIL_AT") == stage:
        raise PlanError(f"injected generation failure: {stage}")


def _plan(args: argparse.Namespace) -> None:
    directory = Path(args.out_dir)
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "grevir_irq_ready.json").unlink(missing_ok=True)
    _write(directory / ".grevir_irq_attempt", (args.attempt + "\n").encode())
    _fail("before_json")
    record = extract_interrupt_plan(Path(args.object).read_bytes())
    plan = decode(record)
    for field in ("backend", "target", "board", "compiler"):
        expected = getattr(args, field)
        if plan[field] != expected:
            raise PlanError(f"interrupt {field} differs from build configuration")
    _write(directory / f"grevir_generated_irq_plan_{plan['backend']}.json",
           canonical_bytes(plan))
    _fail("after_json")


def _emit(args: argparse.Namespace) -> None:
    directory = Path(args.out_dir)
    marker = directory / "grevir_irq_ready.json"
    marker.unlink(missing_ok=True)
    attempt_file = directory / ".grevir_irq_attempt"
    if not attempt_file.exists() or attempt_file.read_text() != args.attempt + "\n":
        raise PlanError("interrupt generation attempt mismatch")
    plan_path = directory / f"grevir_generated_irq_plan_{args.backend}.json"
    plan_bytes = plan_path.read_bytes()
    plan = parse_canonical(plan_bytes)
    if plan["backend"] != args.backend:
        raise PlanError("interrupt emitter backend differs from plan")
    if plan["compiler"] != args.compiler:
        raise PlanError("interrupt emitter compiler differs from plan")
    header, source = emit(plan, args.application_header)
    header_path = directory / f"grevir_generated_irq_bindings_{args.backend}.hpp"
    source_path = directory / f"grevir_generated_irq_bindings_{args.backend}.cpp"
    _write(header_path, header)
    _fail("after_header")
    _write(source_path, source)
    _fail("after_source")
    ready = {
        "attempt": args.attempt,
        "backend": args.backend,
        "target": plan["target"],
        "board": plan["board"],
        "compiler": plan["compiler"],
        "emitter": EMITTER,
        "protocol": plan["schema"],
        "plan_fingerprint": plan["fingerprint"],
        "plan_sha256": hashlib.sha256(plan_bytes).hexdigest(),
        "output_identity": output_identity(header, source),
    }
    _fail("before_marker")
    _write(marker, (json.dumps(ready, sort_keys=True, indent=2) + "\n").encode())


def main() -> int:
    if len(sys.argv) > 1 and sys.argv[1] == "arduino-build":
        try:
            return arduino_build_main(sys.argv[2:])
        except (OSError, subprocess.CalledProcessError, PlanError) as error:
            print(f"grevir-irqgen: {error}", file=sys.stderr)
            return 1
    parser = argparse.ArgumentParser(
        prog="grevir-irqgen",
        description="Generate Grevir interrupt bindings; see docs/guides/interrupts.md")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("arduino-build", help="probe and build a staged Arduino sketch")
    plan = commands.add_parser("plan")
    plan.add_argument("--object", required=True)
    plan.add_argument("--out-dir", required=True)
    plan.add_argument("--attempt", required=True)
    for field in ("backend", "target", "board", "compiler"):
        plan.add_argument(f"--{field}", required=True)
    emit_command = commands.add_parser("emit")
    emit_command.add_argument("--out-dir", required=True)
    emit_command.add_argument("--attempt", required=True)
    emit_command.add_argument("--backend", required=True)
    emit_command.add_argument("--compiler", required=True)
    emit_command.add_argument("--application-header", required=True)
    args = parser.parse_args()
    try:
        if args.command == "plan":
            _plan(args)
        else:
            _emit(args)
    except (OSError, ObjectSectionError, PlanError) as error:
        print(f"grevir-irqgen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
