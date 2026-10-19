"""Compare the production probe with frozen/manual Shape2 wire expectations.

The tiny TLV reader below is independent of protobuf/the candidate codec. It
only mutates fixture fields and checks raw frozen bytes; it does not bless or
rewrite a Golden. Non-wire SH2 cases belong to their runtime/geometry oracles.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "verification/corpus/semantic/shape2-triangle/v1"


def varint(value: int) -> bytes:
    result = bytearray()
    while value >= 128:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def read_varint(data: bytes, offset: int) -> tuple[int, int]:
    value, shift = 0, 0
    while offset < len(data) and shift < 70:
        byte = data[offset]
        offset += 1
        value |= (byte & 127) << shift
        if not byte & 128:
            return value, offset
        shift += 7
    raise ValueError("truncated/oversized reference varint")


def fields(data: bytes) -> list[tuple[int, int, bytes]]:
    result, offset = [], 0
    while offset < len(data):
        key, offset = read_varint(data, offset)
        tag, wire = key >> 3, key & 7
        start = offset
        if wire == 0:
            _, offset = read_varint(data, offset)
        elif wire in (1, 5):
            offset += 8 if wire == 1 else 4
        elif wire == 2:
            length, offset = read_varint(data, offset)
            start = offset
            offset += length
        else:
            raise ValueError("unsupported reference wire type")
        if not tag or offset > len(data):
            raise ValueError("invalid reference length/tag")
        result.append((tag, wire, data[start:offset]))
    return result


def encode(parts: list[tuple[int, int, bytes]]) -> bytes:
    return b"".join(varint(tag << 3 | wire) + (varint(len(value)) if wire == 2 else b"") + value
                    for tag, wire, value in parts)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    manifest = json.loads((CORPUS / "manifest.json").read_text())
    rows, originals = [], {}

    def observe(name: str, root_type: str, data: bytes, expected: bool, basis: str) -> None:
        path = output / (name + ".bin")
        path.write_bytes(data)
        command = [str(args.probe.resolve()), "--root-type", root_type, "--input", str(path)]
        result = subprocess.run(command, capture_output=True, text=True)
        actual = json.loads(result.stdout) if result.returncode == 0 else None
        passed = actual is not None and actual["accepted"] == expected
        if expected:
            passed = passed and actual["canonicalHex"] == data.hex()
        rows.append({"id": name, "root_type": root_type, "basis": basis, "command": command,
                     "input_sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
                     "expected_accepted": expected, "actual": actual,
                     "exit_code": result.returncode, "stderr": result.stderr,
                     "result": "PASS" if passed else "FAIL"})

    for vector in manifest["vectors"]:
        data = bytes.fromhex((CORPUS / vector["path"]).read_text())
        assert len(data) == vector["byte_length"]
        assert hashlib.sha256(data).hexdigest() == vector["sha256"]
        assert encode(fields(data)) == data
        originals[vector["name"]] = data
        observe("SH2-G15-" + vector["name"], vector["name"], data, True, "frozen manifest raw bytes/hash")

    shape = fields(originals["ShapeContent"])
    for missing in (1, 2, 3):
        observe(f"SH2-G11-missing-{missing}", "ShapeContent",
                encode([part for part in shape if part[0] != missing]), False, "SH2-G11 required scalar presence")
    for tag, wire, value in shape:
        observe(f"SH2-G25-duplicate-{tag}", "ShapeContent",
                originals["ShapeContent"] + encode([(tag, wire, value)]), False, "SH2-G25 duplicate singular")
    observe("SH2-G25-unknown", "ShapeContent", originals["ShapeContent"] + b"\x20\x01", False, "SH2-G25 unknown field")
    observe("SH2-G25-truncated", "ShapeContent", originals["ShapeContent"][:-1], False, "SH2-G25 malformed wire")
    for tag in (2, 3):
        for name, value in [("zero", 0.0), ("negative", -1.0), ("negative-zero", -0.0),
                            ("nan", float("nan")), ("infinity", float("inf"))]:
            data = encode([(t, w, struct.pack("<d", value) if t == tag else v) for t, w, v in shape])
            observe(f"SH2-G09-G10-{tag}-{name}", "ShapeContent", data, False, "SH2-G09/G10 positive finite extent")
    obj = fields(originals["ObjectRecord"])
    for version in (1, 3):
        data = encode([(tag, wire, varint(version) if tag == 3 else value) for tag, wire, value in obj])
        observe(f"SH2-G06-G08-version-{version}", "ObjectRecord", data, False, "Shape1/kind3 and unknown Shape3 reject")
    operation = fields(originals["Operation"])
    for tag in (3, 4):
        data = encode([(t, w, varint(2) if t == tag else v) for t, w, v in operation])
        observe(f"SH2-envelope-{tag}-v2", "Operation", data, False, "FC01-B01 Operation/payload remain v1")
    report = {"source_revision": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
              "working_tree_changes": subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).splitlines(),
              "probe_sha256": hashlib.sha256(args.probe.read_bytes()).hexdigest(),
              "frozen_manifest_sha256": hashlib.sha256((CORPUS / "manifest.json").read_bytes()).hexdigest(),
              "cases": rows, "status": "PASS" if all(row["result"] == "PASS" for row in rows) else "FAIL",
              "full_SH2_G01_to_G25_claim": False, "physical_claim": False, "gate_claim": None}
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(report["status"], len(rows), flush=True)
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
