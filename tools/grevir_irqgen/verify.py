"""Validate a complete current Grevir interrupt generation attempt."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

from emit import EMITTER, output_identity
from protocol import PlanError, SCHEMA, parse_canonical


def verify_ready(directory: Path, backend: str, target: str, board: str,
                 compiler: str, attempt: str | None = None) -> dict:
    marker_path = directory / "grevir_irq_ready.json"
    try:
        marker = json.loads(marker_path.read_text())
        recorded_attempt = (directory / ".grevir_irq_attempt").read_text().strip()
        plan_bytes = (directory / f"grevir_generated_irq_plan_{backend}.json").read_bytes()
        header = (directory / f"grevir_generated_irq_bindings_{backend}.hpp").read_bytes()
        source = (directory / f"grevir_generated_irq_bindings_{backend}.cpp").read_bytes()
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise PlanError("interrupt output set is incomplete") from error
    expected_fields = {"attempt", "backend", "target", "board", "compiler",
                       "emitter", "protocol", "plan_fingerprint", "plan_sha256",
                       "output_identity"}
    if type(marker) is not dict or set(marker) != expected_fields:
        raise PlanError("interrupt ready marker fields differ")
    if (not isinstance(marker["attempt"], str)
            or not marker["attempt"]
            or marker["attempt"] != recorded_attempt
            or (attempt is not None and marker["attempt"] != attempt)):
        raise PlanError("interrupt generation attempt differs")
    if any(marker[field] != value for field, value in
           (("backend", backend), ("target", target), ("board", board),
            ("compiler", compiler), ("emitter", EMITTER), ("protocol", SCHEMA))):
        raise PlanError("interrupt ready marker identity differs")
    plan = parse_canonical(plan_bytes)
    if any(plan[field] != value for field, value in
           (("backend", backend), ("target", target), ("board", board),
            ("compiler", compiler))):
        raise PlanError("interrupt plan identity differs")
    if (marker["plan_fingerprint"] != plan["fingerprint"]
            or marker["plan_sha256"] != hashlib.sha256(plan_bytes).hexdigest()
            or marker["output_identity"] != output_identity(header, source)):
        raise PlanError("interrupt generated outputs differ from ready marker")
    return marker


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify a generated interrupt plan and binding unit before compilation")
    for name in ("out-dir", "backend", "target", "board", "compiler"):
        parser.add_argument(f"--{name}", required=True)
    parser.add_argument("--attempt")
    args = parser.parse_args()
    try:
        verify_ready(Path(args.out_dir), args.backend, args.target, args.board,
                     args.compiler, args.attempt)
    except (OSError, PlanError) as error:
        print(f"grevir-irqgen: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
