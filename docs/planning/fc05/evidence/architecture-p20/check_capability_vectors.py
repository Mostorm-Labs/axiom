"""Validate FC05 capability successor fixtures without importing production code."""
from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent
VECTORS = ROOT / "capability-vectors.json"


def main() -> None:
    data = json.loads(VECTORS.read_text(encoding="utf-8"))
    assert data["status"] == "SUCCESSOR_REVIEW_INPUT_NOT_A_PASS"
    seen = set()
    for vector in data["vectors"]:
        assert vector["id"] not in seen, vector["id"]
        seen.add(vector["id"])
        ids = vector["required_ids"]
        assert len(ids) == len(set(ids)), vector["id"]
        assert ids == sorted(ids), (vector["id"], ids)
        assert "resource-manifest-v1" in ids, vector["id"]
        attached = "connector_attached_shape2" in vector["features"] or "connector_attached_shape3" in vector["features"]
        assert ("connector-shape-attachment-v1" in ids) == attached, vector["id"]
    required_negative = {
        "unknown_required_id",
        "duplicate_required_id",
        "unsorted_required_ids",
        "missing_connector_shape_attachment_for_shape2_or_shape3_target",
        "old_reader_without_connector_shape_attachment_v1",
        "continuation_without_sticky_edit_v1_support",
    }
    assert set(data["negative_vectors"]) == required_negative
    print(json.dumps({"result": "PASS", "vectors": len(seen), "classification": "design fixture check; not Runtime or Gate PASS"}))


if __name__ == "__main__":
    main()
