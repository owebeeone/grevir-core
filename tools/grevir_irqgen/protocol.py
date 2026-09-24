"""Versioned, pointer-free Grevir interrupt probe record and canonical plan."""

from __future__ import annotations

import hashlib
import json
import struct


class PlanError(ValueError):
    """A probe record or canonical plan violates the interrupt contract."""


SCHEMA = 1
BACKENDS = {"mock", "avr", "esp32"}
TEXT_FIELDS = ("owner", "configuration", "source", "selector", "entry", "snapshot_policy",
               "acknowledge_policy")


class Reader:
    def __init__(self, data: bytes):
        self.data = data
        self.position = 0

    def take(self, size: int) -> bytes:
        if size < 0 or size > len(self.data) - self.position:
            raise PlanError("truncated interrupt record")
        result = self.data[self.position:self.position + size]
        self.position += size
        return result

    def byte(self) -> int:
        return self.take(1)[0]

    def word(self) -> int:
        return struct.unpack("<H", self.take(2))[0]

    def dword(self) -> int:
        return struct.unpack("<I", self.take(4))[0]

    def text(self) -> str:
        raw = self.take(self.byte())
        try:
            return raw.decode("utf-8")
        except UnicodeDecodeError as error:
            raise PlanError("invalid UTF-8 interrupt key") from error


def _identifier(value: str) -> bool:
    return bool(value) and all(character.isascii() and
                               (character.isalnum() or character == "_")
                               for character in value)


def _event(reader: Reader) -> dict[str, str]:
    return {"instance": reader.text(), "request": reader.text(),
            "kind": reader.text()}


def _key(event: dict[str, str]) -> tuple[str, str, str]:
    return (event["instance"], event["request"], event["kind"])


def _validate_plan(plan: dict) -> dict:
    if type(plan) is not dict or set(plan) != {
        "schema", "backend", "target", "board", "compiler", "application",
        "demands", "bindings", "source_groups", "fingerprint"
    }:
        raise PlanError("invalid interrupt plan fields")
    if plan["schema"] != SCHEMA or type(plan["schema"]) is not int:
        raise PlanError("unsupported interrupt schema")
    if type(plan["backend"]) is not str or plan["backend"] not in BACKENDS:
        raise PlanError("unsupported interrupt backend")
    for field in ("backend", "target", "board", "compiler", "application"):
        if type(plan[field]) is not str or not _identifier(plan[field]):
            raise PlanError(f"invalid interrupt {field}")
    demands = plan["demands"]
    bindings = plan["bindings"]
    if type(demands) is not list or type(bindings) is not list:
        raise PlanError("invalid interrupt event lists")
    if len(demands) != len(bindings):
        raise PlanError("interrupt demands and bindings differ")
    demand_keys: list[tuple[str, str, str]] = []
    for event in demands:
        if type(event) is not dict or set(event) != {"instance", "request", "kind"}:
            raise PlanError("invalid interrupt event")
        if any(type(value) is not str or not _identifier(value)
               for value in event.values()):
            raise PlanError("invalid interrupt event key")
        demand_keys.append(_key(event))
    if demand_keys != sorted(set(demand_keys)):
        raise PlanError("interrupt demands are duplicated or unordered")
    binding_keys: list[tuple[str, str, str]] = []
    groups: dict[str, dict] = {}
    entries: dict[str, str] = {}
    for binding in bindings:
        required = {"event", *TEXT_FIELDS, "dispatch_order", "shared_source"}
        if type(binding) is not dict or set(binding) != required:
            raise PlanError("invalid interrupt binding")
        event = binding["event"]
        if type(event) is not dict or set(event) != {"instance", "request", "kind"}:
            raise PlanError("invalid binding event")
        if any(type(value) is not str or not _identifier(value)
               for value in event.values()):
            raise PlanError("invalid binding event key")
        binding_keys.append(_key(event))
        for field in TEXT_FIELDS:
            if type(binding[field]) is not str or not _identifier(binding[field]):
                raise PlanError(f"invalid binding {field}")
        order = binding["dispatch_order"]
        if type(order) is not int or not 0 <= order <= 65535:
            raise PlanError("invalid dispatch order")
        if type(binding["shared_source"]) is not bool:
            raise PlanError("invalid shared-source flag")
        if binding["shared_source"]:
            raise PlanError("shared interrupt sources are not implemented")
        source = binding["source"]
        entry = binding["entry"]
        if entry in entries and entries[entry] != source:
            raise PlanError("exclusive interrupt entry conflict")
        entries[entry] = source
        if source not in groups:
            groups[source] = {
                "source": source, "owner": binding["owner"], "entry": entry,
                "snapshot_policy": binding["snapshot_policy"],
                "acknowledge_policy": binding["acknowledge_policy"],
                "events": []
            }
        group = groups[source]
        for field in ("owner", "entry", "snapshot_policy", "acknowledge_policy"):
            if group[field] != binding[field]:
                raise PlanError("interrupt source has conflicting owners or policies")
        if group["events"]:
            raise PlanError("shared interrupt sources are not implemented")
        group["events"].append({"event": event, "selector": binding["selector"],
                                "dispatch_order": order})
    if binding_keys != demand_keys:
        raise PlanError("interrupt bindings do not equal demands")
    canonical_groups = [groups[key] for key in sorted(groups)]
    for group in canonical_groups:
        group["events"].sort(key=lambda item: (item["dispatch_order"],
                                                _key(item["event"])))
    if plan["source_groups"] != canonical_groups:
        raise PlanError("interrupt source groups do not match bindings")
    unsigned = {key: value for key, value in plan.items() if key != "fingerprint"}
    fingerprint = hashlib.sha256(_json(unsigned)).hexdigest()
    if plan["fingerprint"] != fingerprint:
        raise PlanError("interrupt plan fingerprint mismatch")
    return plan


def _json(value: dict) -> bytes:
    return (json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True) + "\n").encode()


def decode(data: bytes) -> dict:
    """Decode and validate one complete target-compiled probe record."""
    reader = Reader(data)
    if reader.take(4) != b"GIRQ":
        raise PlanError("invalid interrupt record magic")
    schema = reader.word()
    if schema != SCHEMA:
        raise PlanError("unsupported interrupt schema")
    length = reader.dword()
    if length != len(data):
        raise PlanError("interrupt record length mismatch")
    demand_count = reader.word()
    binding_count = reader.word()
    header = {field: reader.text() for field in
              ("backend", "target", "board", "compiler", "application")}
    demands = [_event(reader) for _ in range(demand_count)]
    bindings = []
    for _ in range(binding_count):
        event = _event(reader)
        binding = {"event": event}
        binding.update({field: reader.text() for field in TEXT_FIELDS})
        binding["dispatch_order"] = reader.word()
        flag = reader.byte()
        if flag not in (0, 1):
            raise PlanError("invalid shared-source flag")
        binding["shared_source"] = bool(flag)
        bindings.append(binding)
    if reader.position != len(data) - 4:
        raise PlanError("interrupt record trailing bytes")
    expected = reader.dword()
    actual = 2166136261
    for byte in data[:-4]:
        actual = ((actual ^ byte) * 16777619) & 0xffffffff
    if expected != actual:
        raise PlanError("interrupt record checksum mismatch")
    groups: dict[str, dict] = {}
    for binding in bindings:
        source = binding["source"]
        group = groups.setdefault(source, {
            "source": source, "owner": binding["owner"], "entry": binding["entry"],
            "snapshot_policy": binding["snapshot_policy"],
            "acknowledge_policy": binding["acknowledge_policy"], "events": []
        })
        group["events"].append({"event": binding["event"],
                                "selector": binding["selector"],
                                "dispatch_order": binding["dispatch_order"]})
    source_groups = [groups[key] for key in sorted(groups)]
    for group in source_groups:
        group["events"].sort(key=lambda item: (item["dispatch_order"],
                                                _key(item["event"])))
    plan = {"schema": schema, **header, "demands": demands,
            "bindings": bindings, "source_groups": source_groups}
    plan["fingerprint"] = hashlib.sha256(_json(plan)).hexdigest()
    return _validate_plan(plan)


def canonical_bytes(plan: dict) -> bytes:
    """Reject tampering, then return a byte-for-byte stable JSON encoding."""
    return _json(_validate_plan(plan))


def parse_canonical(data: bytes) -> dict:
    try:
        plan = json.loads(data)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise PlanError("invalid interrupt plan JSON") from error
    if canonical_bytes(plan) != data:
        raise PlanError("interrupt plan JSON is not canonical")
    return plan
