"""Read-only parser for legacy Procreate .brushset archives.

The parser intentionally keeps the decoded NSKeyedArchiver values as data. It
does not attempt to emulate Procreate's renderer or infer missing parameters.
"""

from __future__ import annotations

import base64
import hashlib
import json
import plistlib
import struct
import zipfile
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any


def _png_dimensions(data: bytes) -> tuple[int, int]:
    if data[:8] != b"\x89PNG\r\n\x1a\n" or len(data) < 24:
        raise ValueError("resource_is_not_png")
    return struct.unpack(">II", data[16:24])


def _json_value(value: Any, objects: list[Any], depth: int = 0) -> Any:
    if depth > 32:
        raise ValueError("archive_reference_depth")
    if isinstance(value, plistlib.UID):
        return _json_value(objects[value.data], objects, depth + 1)
    if isinstance(value, dict):
        return {str(k): _json_value(v, objects, depth + 1) for k, v in value.items()}
    if isinstance(value, list):
        return [_json_value(v, objects, depth + 1) for v in value]
    if isinstance(value, bytes):
        return {"encoding": "base64", "value": base64.b64encode(value).decode("ascii")}
    if isinstance(value, str) and value == "$null":
        # Spectator presents Procreate's archive sentinel as JSON null. Keep
        # the report aligned with that decoded representation; the sibling
        # Shape.png/Grain.png files remain the authoritative resource payload.
        return None
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    return str(value)


@dataclass(frozen=True)
class BrushsetResult:
    name: str
    uuid: str
    source_sha256: str
    archive_sha256: str
    parameters: dict[str, Any]
    shape_path: str
    grain_path: str
    shape_sha256: str
    grain_sha256: str
    shape_dimensions: tuple[int, int]
    grain_dimensions: tuple[int, int]
    preview_path: str
    preview_sha256: str
    preview_dimensions: tuple[int, int]

    def to_dict(self) -> dict[str, Any]:
        value = asdict(self)
        value["shape_dimensions"] = list(self.shape_dimensions)
        value["grain_dimensions"] = list(self.grain_dimensions)
        value["preview_dimensions"] = list(self.preview_dimensions)
        return value


def _archive_root(raw: bytes) -> dict[str, Any]:
    decoded = plistlib.loads(raw)
    objects = decoded["$objects"]
    root = decoded["$top"]["root"]
    result = _json_value(objects[root.data], objects)
    if not isinstance(result, dict):
        raise ValueError("archive_root_is_not_dictionary")
    return result


def parse_brushset(path: Path | str, brush_name: str) -> BrushsetResult:
    source = Path(path)
    source_bytes = source.read_bytes()
    source_digest = hashlib.sha256(source_bytes).hexdigest()
    with zipfile.ZipFile(source) as archive:
        brushset = plistlib.loads(archive.read("brushset.plist"))
        for uuid in brushset.get("brushes", []):
            prefix = f"{uuid}/"
            archive_raw = archive.read(prefix + "Brush.archive")
            parameters = _archive_root(archive_raw)
            name = parameters.get("name")
            if name != brush_name:
                continue
            shape_path = prefix + "Shape.png"
            grain_path = prefix + "Grain.png"
            preview_path = prefix + "QuickLook/Thumbnail.png"
            shape = archive.read(shape_path)
            grain = archive.read(grain_path)
            preview = archive.read(preview_path)
            return BrushsetResult(
                name=name,
                uuid=uuid,
                source_sha256=source_digest,
                archive_sha256=hashlib.sha256(archive_raw).hexdigest(),
                parameters=parameters,
                shape_path=shape_path,
                grain_path=grain_path,
                shape_sha256=hashlib.sha256(shape).hexdigest(),
                grain_sha256=hashlib.sha256(grain).hexdigest(),
                shape_dimensions=_png_dimensions(shape),
                grain_dimensions=_png_dimensions(grain),
                preview_path=preview_path,
                preview_sha256=hashlib.sha256(preview).hexdigest(),
                preview_dimensions=_png_dimensions(preview),
            )
    raise ValueError(f"brush_not_found:{brush_name}")


def write_report(path: Path | str, brushset: Path | str, brush_name: str) -> None:
    result = parse_brushset(brushset, brush_name)
    Path(path).write_text(json.dumps(result.to_dict(), sort_keys=True, indent=2) + "\n")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("brushset", type=Path)
    parser.add_argument("--name", default="Membrane")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    write_report(args.report, args.brushset, args.name)
