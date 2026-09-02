#!/usr/bin/env python3
"""Fail-closed drift guard for the GT-G1-04-A machine projections."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
NORMALIZER = ROOT / "runtime/semantic/include/canvas/semantic/normalizer.hpp"
OP_PROFILE = ROOT / "schema/axiom/v1/canonical/operation_structural_profile_v1.yaml"
LEAF_PROFILE = ROOT / "schema/axiom/v1/canonical/semantic_leaf_constraints_v1.yaml"
KIND_PROFILE = ROOT / "schema/axiom/v1/registry/object_kind_registry_v1.yaml"

EXPECTED_COLLECTIONS = [
    ("payload.insert_objects.objects", "id"),
    ("payload.delete_objects.object_ids", "object_id"),
    ("payload.restore_objects.objects", "id"),
    ("payload.set_placements.items", "object_id"),
    ("payload.set_transforms.items", "object_id"),
    ("payload.patch_properties.patches", "(object_id, field_id)"),
    ("payload.set_object_size.items", "object_id"),
    ("payload.split_strokes.splits", "source_stroke_id"),
    ("payload.split_strokes.splits.replacements", "id"),
    ("payload.add_erase_masks.items", "object_id"),
    ("payload.add_erase_masks.items.masks", "mask_id"),
    ("payload.remove_erase_masks.items", "object_id"),
    ("payload.remove_erase_masks.items.mask_ids", "mask_id"),
]
EXPECTED_WIDE = {
    "payload.split_strokes.splits.replacements",
    "payload.add_erase_masks.items.masks",
}


def fail(message: str) -> int:
    print(f"projection contract: FAIL: {message}", file=sys.stderr)
    return 1


def main() -> int:
    for path in (NORMALIZER, OP_PROFILE, LEAF_PROFILE, KIND_PROFILE):
        if not path.is_file():
            return fail(f"missing input {path}")
    source = NORMALIZER.read_text(encoding="utf-8")
    declared = re.findall(r'\{"([^\"]+)",\s*"([^\"]+)"\}', source)
    if declared != EXPECTED_COLLECTIONS:
        return fail(f"collection inventory mismatch: {declared!r}")
    wide = set(re.findall(r'"(payload\.[^"]+)"', source[source.find("kOperationWideUniquePaths"):]))
    if wide != EXPECTED_WIDE:
        return fail(f"operation-wide uniqueness mismatch: {wide!r}")

    operation_profile = OP_PROFILE.read_text(encoding="utf-8")
    for path, key in EXPECTED_COLLECTIONS:
        yaml_key = "[object_id, field_id]" if key == "(object_id, field_id)" else key
        if f'path: {path}, key: {yaml_key}' not in operation_profile:
            return fail(f"authority collection missing {path}/{key}")
    if "operation_wide_uniqueness:" not in operation_profile:
        return fail("authority operation-wide uniqueness section missing")

    kind_profile = KIND_PROFILE.read_text(encoding="utf-8")
    released = re.findall(r"- \{id: (\d+), symbol: [^,]+, content_branch: [^,]+, current_version: (\d+), accepted_versions: \[(\d+)\]\}", kind_profile)
    if released != [(str(index), "1", "1") for index in range(1, 10)]:
        return fail(f"released ObjectKind registry mismatch: {released!r}")
    leaf_profile = LEAF_PROFILE.read_text(encoding="utf-8")
    required_markers = (
        "min_items: 1", "first: move_to", "close_requires_open_subpath: true",
        "after_close_requires_move_to: true", "oneof_command: exactly_one",
        "delta_version: {accepted: [1], missing: reject, zero: reject, unknown: reject}",
        "steps: {min_items: 1, order: ordered_sequence, duplicate_removal: forbidden}",
        "oneof_step: exactly_one", "vector_samples: {min_items: 1, order: ordered_sequence}",
        "dab_dabs: {min_items: 1, order: ordered_sequence}", "size: finite_positive",
        "opacity: finite_0_1",
    )
    for marker in required_markers:
        if marker not in leaf_profile:
            return fail(f"leaf authority marker missing: {marker}")
    print("projection contract: PASS (13 collections, 2 operation-wide uniqueness rules, 9 ObjectKinds, leaf markers)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
