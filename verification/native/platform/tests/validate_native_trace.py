#!/usr/bin/env python3
"""Run native hook/common-host probes and validate their structured traces."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess


class TraceError(RuntimeError):
    """A native verification trace is missing or violates the contract."""


def run(executable: Path, output: Path) -> None:
    result = subprocess.run(
        [str(executable), "--trace", str(output)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        raise TraceError(f"probe failed ({result.returncode}): {executable}\n{result.stdout}")
    if not output.is_file():
        raise TraceError(f"probe did not create trace: {output}")


def load(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise TraceError(f"invalid JSON trace {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise TraceError(f"trace root is not an object: {path}")
    return value


def validate_hooks(trace: dict) -> None:
    if trace.get("schema_version") != 1 or trace.get("trace_kind") != "verification-native-hooks":
        raise TraceError("hooks trace identity mismatch")
    if trace.get("normalized_loss") != [
        {"fault_id": "fault:surface", "generation": 7, "state": "CLEARED", "type": "SURFACE_LOST"},
        {"fault_id": "fault:device", "generation": 8, "state": "CLEARED", "type": "DEVICE_LOST"},
    ]:
        raise TraceError("normalized surface/device loss trace mismatch")
    hold = trace.get("present_hold")
    if hold != {
        "captured": ["attempt:held:1", "attempt:held:2"],
        "release_order": [1, 2],
        "release_dispositions": ["DROPPED_STALE_GENERATION", "DROPPED_STALE_GENERATION"],
    }:
        raise TraceError("present hold/release trace mismatch")
    attempts = trace.get("source_attempts")
    if attempts != [
        {"attempt_id": "attempt:held:1", "disposition": "DROPPED_STALE_GENERATION", "generation": 3},
        {"attempt_id": "attempt:held:2", "disposition": "DROPPED_STALE_GENERATION", "generation": 3},
        {"attempt_id": "attempt:stale", "disposition": "DROPPED_STALE_GENERATION", "generation": 3},
        {"attempt_id": "attempt:current", "disposition": "FORWARDED", "generation": 4},
        {"attempt_id": "attempt:immediate", "disposition": "FORWARDED", "generation": 4},
        {"attempt_id": "attempt:stale-scope", "disposition": "DROPPED_STALE_SCOPE", "generation": 4},
    ]:
        raise TraceError("source-attempt trace mismatch")
    if trace.get("event_tap") != [
        {"generation": 4, "source_lease_id": "source:present:1"},
        {"generation": 4, "source_lease_id": "source:present:1"},
    ]:
        raise TraceError("stale attempt leaked into EventTap or current event is absent")
    if trace.get("closed_scope") != {"all_leases_closed": True, "late_lease_rejected": True}:
        raise TraceError("closed-scope trace mismatch")


def validate_host(trace: dict) -> None:
    if trace.get("schema_version") != 1 or trace.get("trace_kind") != "verification-platform-host-common":
        raise TraceError("common-host trace identity mismatch")
    if trace.get("build_manifest") != {
        "product_public_abi": False,
        "protocol_version": 1,
        "verification_only": True,
    }:
        raise TraceError("verification build manifest mismatch")
    if trace.get("profile") != {
        "capabilities": ["fault.surface.loss", "fault.device.loss", "harness.source_attempt"],
        "platform_family": "HEADLESS",
        "profile_id": "headless-reference-v0.1",
    }:
        raise TraceError("profile/capability trace mismatch")
    facts = trace.get("facts")
    if not isinstance(facts, list) or not facts:
        raise TraceError("common host emitted no protocol facts")
    forbidden = {"SCENARIO_EXPECTED", "COMPARISON_RESULT"}
    if any(fact.get("kind") in forbidden for fact in facts if isinstance(fact, dict)):
        raise TraceError("common host crossed the expected/comparator boundary")
    if trace.get("rejections") != [
        "DUPLICATE_COMPLETION",
        "UNKNOWN_ACTION_BINDING",
        "ACTION_AFTER_CLOSE",
        "REUSED_SESSION_EPOCH",
    ]:
        raise TraceError("common-host rejection trace mismatch")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hooks", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        args.output.mkdir(parents=True, exist_ok=True)
        hooks_path = args.output / "native-hooks-trace.json"
        host_path = args.output / "platform-host-common-trace.json"
        run(args.hooks.resolve(), hooks_path)
        run(args.host.resolve(), host_path)
        validate_hooks(load(hooks_path))
        validate_host(load(host_path))
    except (OSError, TraceError) as exc:
        print(f"native trace validation failed: {exc}")
        return 1
    print(f"native trace: valid ({hooks_path}, {host_path})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
