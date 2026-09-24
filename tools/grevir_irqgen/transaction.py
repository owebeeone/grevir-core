"""One generation attempt for a CMake or staged-Arduino build."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import uuid

from verify import verify_ready
from protocol import PlanError


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("object", "out-dir", "backend", "target", "board",
                 "compiler", "application-header"):
        parser.add_argument(f"--{name}", required=True)
    args = parser.parse_args()
    directory = Path(args.out_dir)
    directory.mkdir(parents=True, exist_ok=True)
    marker = directory / "grevir_irq_ready.json"
    marker.unlink(missing_ok=True)
    attempt = uuid.uuid4().hex
    tool = Path(__file__).with_name("__main__.py")
    commands = [
        ("plan", "--object", args.object, "--out-dir", str(directory),
         "--attempt", attempt, "--backend", args.backend, "--target",
         args.target, "--board", args.board, "--compiler", args.compiler),
        ("emit", "--out-dir", str(directory), "--attempt", attempt,
         "--backend", args.backend, "--compiler", args.compiler,
         "--application-header", args.application_header),
    ]
    for command in commands:
        result = subprocess.run((sys.executable, "-B", str(tool), *command),
                                check=False)
        if result.returncode != 0:
            return result.returncode
    try:
        verify_ready(directory, args.backend, args.target, args.board,
                     args.compiler, attempt)
    except (OSError, PlanError) as error:
        marker.unlink(missing_ok=True)
        print(f"grevir-irqgen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
