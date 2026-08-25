#!/usr/bin/env python3
"""Build-independent Android instrumentation launcher for GT-G0-12."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


def adb(serial: str, *args: str) -> str:
    return subprocess.run(
        ["adb", "-s", serial, *args],
        check=True,
        text=True,
        capture_output=True,
    ).stdout


def choose_serial(requested: str | None) -> str:
    rows = []
    for line in subprocess.run(
        ["adb", "devices", "-l"], check=True, text=True, capture_output=True
    ).stdout.splitlines()[1:]:
        fields = line.split()
        if len(fields) >= 2 and fields[1] == "device":
            rows.append(fields[0])
    if requested:
        if requested not in rows:
            raise RuntimeError(f"Android device {requested!r} is not online")
        return requested
    if len(rows) != 1:
        raise RuntimeError(f"expected exactly one Android device, found {len(rows)}")
    return rows[0]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--app-apk", type=Path, required=True)
    parser.add_argument("--test-apk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--serial")
    args = parser.parse_args()
    serial = choose_serial(args.serial)
    args.output.mkdir(parents=True, exist_ok=True)
    app_apk = args.app_apk.resolve(strict=True)
    test_apk = args.test_apk.resolve(strict=True)
    subprocess.run(["adb", "-s", serial, "install", "-r", str(app_apk)], check=True)
    subprocess.run(["adb", "-s", serial, "install", "-r", str(test_apk)], check=True)
    completed = subprocess.run(
        [
            "adb", "-s", serial, "shell", "am", "instrument", "-w",
            "dev.mostorm.axiom.verification.android.test/"
            "dev.mostorm.axiom.verification.android.HarnessInstrumentation",
        ],
        check=False,
        text=True,
        capture_output=True,
    )
    log = completed.stdout + completed.stderr
    (args.output / "instrumentation-log.txt").write_text(log, encoding="utf-8")
    snapshot_match = re.findall(r"INSTRUMENTATION_RESULT: snapshot=(\{.*\})", log)
    status_match = re.findall(r"INSTRUMENTATION_RESULT: status=([A-Z_]+)", log)
    if not snapshot_match or not status_match:
        raise RuntimeError("instrumentation did not emit structured status and snapshot")
    snapshot = json.loads(snapshot_match[-1])
    status = status_match[-1]
    if status != "HARNESS_STARTED":
        raise RuntimeError(f"Android instrumentation status was {status}")
    qemu = adb(serial, "shell", "getprop", "ro.kernel.qemu").strip() == "1"
    properties = {}
    for name in (
        "ro.product.manufacturer",
        "ro.product.model",
        "ro.product.device",
        "ro.build.version.release",
        "ro.build.version.sdk",
        "ro.product.cpu.abi",
        "ro.hardware.egl",
    ):
        properties[name] = adb(serial, "shell", "getprop", name).strip()
    result = {
        "format": "axiom-android-instrumentation-result-v1",
        "formatVersion": 1,
        "status": status,
        "snapshot": snapshot,
        "device": {
            "emulator": qemu,
            "properties": properties,
            "serialRedacted": True,
        },
        "exitCode": completed.returncode,
    }
    (args.output / "instrumentation-result.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
