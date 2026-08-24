#!/usr/bin/env python3
"""Validate the deterministic Windows adapter trace emitted by the native probe."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-native", action="store_true")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    import subprocess

    result = subprocess.run(
        [str(args.probe), "--trace", str(args.output)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if result.returncode != 0:
        print(result.stdout)
        return 1
    trace = json.loads(args.output.read_text(encoding="utf-8"))
    expected = {
        "schema_version": 1,
        "trace_kind": "verification-windows-native-adapter",
        "profile": {
            "profile_id": "windows-native-reference-v0.1",
            "platform_family": "WINDOWS",
            "backend": "D3D12",
            "arc_enabled": True,
        },
        "generations": {"metrics": 2, "surface": 2, "device": 2},
        "document_continuity": {"attached_after_rebind": True, "semantic_revision": 1},
        "pointer_batch": {"correlation_id": "batch:1", "count": 2},
        "arc_ownership": {"canonical_owner": "AXIOM", "preview_owner": "ARC", "distinct": True},
        "native_surface_ready": args.require_native,
        "destroy_stale": {"disposition": "DROPPED_STALE_SCOPE", "events_after_destroy": 0},
    }
    if trace != expected:
        print("Windows adapter trace mismatch")
        print(json.dumps(trace, sort_keys=True))
        return 1
    print(f"Windows adapter trace: valid ({args.output})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
