#!/usr/bin/env python3
"""Run the short Android POC-01 visual smoke and validate its RGBA output.

This tool intentionally owns neither the POC-01 renderer nor the Android
document model. It installs the existing POC-01 APK, starts the dedicated
CanvasVisualSmokeActivity, pulls the native readback, and emits an independent
visual report. It accepts both a physical device and a hosted emulator; the
execution kind is recorded in the report and is never inferred from the image.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time


WIDTH = 800
HEIGHT = 600
CHANNELS = 4
EXPECTED_DIGEST = "47826449b895ac4f4a57b4f386379775"
TEXT_REGION = (90, 430, 550, 500)
BACKGROUND = (244, 245, 247, 255)
PACKAGE = "dev.mostorm.canvas.poc01"
ACTIVITY = f"{PACKAGE}/dev.mostorm.canvas.CanvasVisualSmokeActivity"
REMOTE_RGBA = f"/sdcard/Android/data/{PACKAGE}/files/android-visual-smoke.rgba"
REPO_ROOT = Path(__file__).resolve().parents[3]
SKIA_COMMIT = "b6d106297ff9ef2ff8094033695d045e87775581"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def analyze_text_region(rgba: bytes) -> dict:
    expected = WIDTH * HEIGHT * CHANNELS
    if len(rgba) != expected:
        raise ValueError(f"RGBA must contain exactly {expected} bytes")
    x0, y0, x1, y1 = TEXT_REGION
    non_background = 0
    dark_pixels = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            offset = (y * WIDTH + x) * CHANNELS
            pixel = tuple(rgba[offset : offset + CHANNELS])
            if pixel != BACKGROUND:
                non_background += 1
            if pixel[0] < 180 and pixel[1] < 180 and pixel[2] < 180 and pixel[3] > 0:
                dark_pixels += 1
    return {
        "region": list(TEXT_REGION),
        "background_rgba": list(BACKGROUND),
        "non_background_pixels": non_background,
        "dark_pixels": dark_pixels,
        "passed": non_background > 0 and dark_pixels > 0,
    }


def compare_visual_gate(expected: bytes, actual: bytes, tolerance: int = 2) -> dict:
    required = WIDTH * HEIGHT * CHANNELS
    if len(expected) != required or len(actual) != required:
        raise ValueError(f"expected and actual must each contain {required} bytes")
    matching = 0
    maximum_channel_delta = 0
    for pixel in range(WIDTH * HEIGHT):
        matches = True
        for channel in range(CHANNELS):
            offset = pixel * CHANNELS + channel
            delta = abs(expected[offset] - actual[offset])
            maximum_channel_delta = max(maximum_channel_delta, delta)
            matches = matches and delta <= tolerance
        matching += int(matches)
    ratio = matching / (WIDTH * HEIGHT)
    return {
        "width": WIDTH,
        "height": HEIGHT,
        "tolerance": tolerance,
        "matching_pixels": matching,
        "total_pixels": WIDTH * HEIGHT,
        "matching_ratio": ratio,
        "maximum_channel_delta": maximum_channel_delta,
        "passed": ratio >= 0.999,
    }


def write_visual_artifacts(expected: Path, actual: Path, artifacts: Path) -> None:
    """Use the reviewed POC-01 comparator to retain expected/actual/diff PNGs."""
    comparator = REPO_ROOT / "pocs/shared_engine/tools/visual_compare.py"
    subprocess.run(
        [
            sys.executable,
            str(comparator),
            "--expected",
            str(expected),
            "--actual",
            str(actual),
            "--artifacts",
            str(artifacts),
            "--backend",
            "ganesh-gles3-android-visual-smoke",
            "--skia-commit",
            SKIA_COMMIT,
        ],
        check=True,
    )


def write_manifest(output: Path, execution_kind: str) -> None:
    files = []
    for path in sorted(output.rglob("*")):
        if not path.is_file() or path.name == "manifest.json":
            continue
        files.append({
            "path": path.relative_to(output).as_posix(),
            "sha256": sha256(path),
            "bytes": path.stat().st_size,
        })
    manifest = {
        "format": "axiom-poc01-android-visual-smoke-manifest-v1",
        "formatVersion": 1,
        "sourceCommit": os.environ.get("AXIOM_EVIDENCE_SOURCE_COMMIT", "WORKTREE"),
        "executionKind": execution_kind,
        "physicalExecution": execution_kind == "physical",
        "files": files,
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def adb(serial: str, *args: str, capture: bool = True, check: bool = True) -> str:
    completed = subprocess.run(
        ["adb", "-s", serial, *args],
        check=check,
        text=True,
        capture_output=capture,
    )
    return completed.stdout.strip() if capture else ""


def choose_serial(requested: str | None) -> str:
    rows = []
    output = subprocess.run(
        ["adb", "devices", "-l"], check=True, text=True, capture_output=True
    ).stdout
    for line in output.splitlines()[1:]:
        fields = line.split()
        if len(fields) >= 2 and fields[1] == "device":
            rows.append(fields[0])
    if requested:
        if requested not in rows:
            raise RuntimeError(f"requested Android device {requested!r} is not online")
        return requested
    if len(rows) != 1:
        raise RuntimeError(f"expected exactly one online Android device, found {len(rows)}")
    return rows[0]


def wait_for_visual_result(serial: str, timeout: float) -> str:
    deadline = time.monotonic() + timeout
    pattern = re.compile(r"CANVAS_POC01_VISUAL_RESULT\s+(\{.*\})")
    while time.monotonic() < deadline:
        log = adb(serial, "logcat", "-d", "-s", "CanvasPOC01:I", "*:S")
        matches = pattern.findall(log)
        if matches:
            return matches[-1]
        if "CANVAS_POC01_VISUAL_FAILURE" in log:
            raise RuntimeError(log)
        time.sleep(0.25)
    raise RuntimeError("timed out waiting for CANVAS_POC01_VISUAL_RESULT")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--serial")
    parser.add_argument(
        "--expected", type=Path,
        default=REPO_ROOT / "pocs/shared_engine/goldens/reference.rgba",
    )
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--execution-kind", choices=("physical", "emulator"), required=True)
    args = parser.parse_args()
    if args.timeout <= 0:
        raise RuntimeError("--timeout must be positive")
    serial = choose_serial(args.serial)
    args.output.mkdir(parents=True, exist_ok=True)
    apk = args.apk.resolve(strict=True)
    expected = args.expected.resolve(strict=True).read_bytes()
    try:
        adb(serial, "install", "-r", str(apk), capture=False)
        adb(serial, "shell", "rm", "-f", REMOTE_RGBA, check=False)
        adb(serial, "logcat", "-c")
        adb(serial, "shell", "am", "force-stop", PACKAGE)
        adb(serial, "shell", "am", "start", "-W", "-n", ACTIVITY)
        raw_result = wait_for_visual_result(serial, args.timeout)
        result = json.loads(raw_result)
        adb(serial, "pull", REMOTE_RGBA, str(args.output / "actual.rgba"), capture=False)
        with (args.output / "screen.png").open("wb") as screen:
            subprocess.run(
                ["adb", "-s", serial, "exec-out", "screencap", "-p"],
                check=True,
                stdout=screen,
            )
        actual_path = args.output / "actual.rgba"
        actual = actual_path.read_bytes()
        visual = compare_visual_gate(expected, actual)
        text = analyze_text_region(actual)
        write_visual_artifacts(args.expected.resolve(strict=True), actual_path, args.output / "visual")
        report = {
            "format": "axiom-poc01-android-visual-smoke-v1",
            "status": "pass" if visual["passed"] and text["passed"] else "fail",
            "executionKind": args.execution_kind,
            "physicalExecution": args.execution_kind == "physical",
            "deviceSerialRecorded": False,
            "backend": result.get("backend", "ganesh-gles3"),
            "digest": result.get("digest"),
            "expectedDigest": EXPECTED_DIGEST,
            "pixelHash": hashlib.sha256(actual).hexdigest(),
            "expectedRgbaSha256": hashlib.sha256(expected).hexdigest(),
            "actualRgbaSha256": hashlib.sha256(actual).hexdigest(),
            "visual": visual,
            "text": text,
            "apkSha256": sha256(apk),
            "skiaCommit": SKIA_COMMIT,
            "privacy": {"redacted": True, "excluded": ["ADB serial", "device serial number"]},
        }
        (args.output / "result.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
        (args.output / "logcat.txt").write_text(
            adb(serial, "logcat", "-d", "-s", "CanvasPOC01:I", "*:S") + "\n"
        )
        write_manifest(args.output, args.execution_kind)
        if result.get("digest") != EXPECTED_DIGEST:
            raise RuntimeError("Android visual smoke document digest differs from fixture")
        if report["status"] != "pass":
            raise RuntimeError("Android visual smoke visual/text gate failed")
        return 0
    finally:
        # The runner must not mutate global display size/density or rotation.
        # CanvasVisualSmokeView owns an 800x600 surface instead.
        pass


if __name__ == "__main__":
    raise SystemExit(main())
