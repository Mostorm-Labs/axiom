import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ASSET_DIR = ROOT / "runtime" / "ink" / "assets" / "chalk-grain-v1"
EXPECTED = {
    "shape-screenshot-extract-v1.png": "4171df4f399e11b7bc44fa9c688c19f7517622b95c845d63eadda91bbd8feaae",
    "grain-screenshot-extract-v1.png": "d6c0834bd8e9992c3ca448ca5747b76db4ec7f79cd3d82167273c7d71e1bd520",
}


def test_screenshot_derived_chalk_assets_are_bound_to_runtime_package():
    manifest = json.loads((ASSET_DIR / "manifest.json").read_text())
    pipeline = json.loads((ASSET_DIR / "pipeline.json").read_text())
    resources = {item["id"]: item for item in manifest["resources"]}

    for filename, digest in EXPECTED.items():
        path = ASSET_DIR / filename
        assert path.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest

    assert resources["chalk-shape-screenshot-extract-v1"]["sha256"] == EXPECTED[
        "shape-screenshot-extract-v1.png"
    ]
    assert resources["chalk-grain-screenshot-extract-v1"]["sha256"] == EXPECTED[
        "grain-screenshot-extract-v1.png"
    ]
    assert pipeline["shape"]["sha256"] == EXPECTED["shape-screenshot-extract-v1.png"]
    assert pipeline["grain"]["sha256"] == EXPECTED["grain-screenshot-extract-v1.png"]
