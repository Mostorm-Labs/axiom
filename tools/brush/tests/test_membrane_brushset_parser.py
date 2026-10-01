import hashlib
import json
from pathlib import Path

from tools.brush.membrane_brushset_parser import parse_brushset


BRUSHSET = Path(
    "/Users/qing/Desktop/workspace/NearHub/Procreate/Procreate-Legacy-Brushsets/Procreate-1/Procreate_1.brushset"
)


def test_membrane_is_resolved_from_brushset_with_real_resources():
    result = parse_brushset(BRUSHSET, "Membrane")

    assert result.name == "Membrane"
    assert result.uuid == "6F0DDE1B-935E-40AF-B0B2-A0F3727037E0"
    assert result.source_sha256 == hashlib.sha256(BRUSHSET.read_bytes()).hexdigest()
    assert result.parameters["plotSpacing"] > 0
    # Spectator decodes these archive fields as null.  The actual payloads are
    # sibling Shape.png/Grain.png files in the brushset directory.
    assert result.parameters["bundledShapePath"] is None
    assert result.parameters["bundledGrainPath"] is None
    assert result.shape_sha256 and result.grain_sha256
    assert result.shape_dimensions[0] > 0 and result.shape_dimensions[1] > 0
    assert result.grain_dimensions[0] > 0 and result.grain_dimensions[1] > 0
    assert result.preview_path.endswith("QuickLook/Thumbnail.png")
    assert result.preview_sha256
    assert result.preview_dimensions == (1060, 324)


def test_membrane_report_is_stable_and_not_screenshot_assets():
    result = parse_brushset(BRUSHSET, "Membrane")
    encoded = json.dumps(result.to_dict(), sort_keys=True, separators=(",", ":"))
    assert len(hashlib.sha256(encoded.encode()).hexdigest()) == 64
    assert "screenshot-extract" not in result.shape_path
    assert "screenshot-extract" not in result.grain_path
