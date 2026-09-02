#!/usr/bin/env python3
"""Deterministically compile the reviewed C1 cases into derived inputs."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CASES = ROOT / "verification/corpus/semantic/v1/g1-04-c/authoring/cases.json"
DEFAULT_EXPECTED = ROOT / "verification/corpus/semantic/v1/g1-04-c/authoring/expected.json"
DEFAULT_SUITE = ROOT / "verification/corpus/semantic/v1/g1-04-c/suites/core.json"
DEFAULT_OUTPUT = ROOT / "verification/corpus/semantic/v1/g1-04-c/generated"
COMPILER_IDENTITY = "g1-04-c-independent-fixture-compiler-v1"
FORMAT = "axiom-g1-04-c-input-v1"
FORMAT_VERSION = 1
F64_NAN = "f64:7ff8000000000000"
F64_POS_INF = "f64:7ff0000000000000"
F64_NEG_INF = "f64:fff0000000000000"
F64_NEG_ZERO = "f64:8000000000000000"


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True, separators=(",", ": ")) + "\n").encode("utf-8")


def _read_json(path: Path) -> tuple[Any, bytes]:
    raw = path.read_bytes()
    return json.loads(raw.decode("utf-8")), raw


def _stable_id(case_id: str, role: str) -> str:
    return hashlib.sha256(f"axiom-g1-04-c:{case_id}:{role}".encode("utf-8")).hexdigest()[:32]


def _order_key(case_id: str, ordinal: int) -> list[int]:
    digest = hashlib.sha256(f"order:{case_id}:{ordinal}".encode("utf-8")).digest()
    return [int(digest[0]) + 1, ordinal]


def _kind_for_family(family: str, case_id: str = "") -> tuple[int, int]:
    if "WRONG-KIND" in case_id:
        if family == "SetObjectSize":
            return 4, 1
        if family in {"SetVectorPathGeometry", "SetImageContent"}:
            return 1, 1
    if case_id == "C1-PATCH-APPLICABILITY":
        return 5, 1
    if case_id in {"C1-ERASE-ADD-CAPABILITY", "C1-CONNECTOR-TARGET-CAPABILITY"}:
        return 1, 1
    if case_id in {"C1-PLACEMENT-STICKY-RICHTEXT"}:
        return 4, 1
    if case_id in {"C1-HIERARCHY-STICKY", "C1-INSERT-STICKY-CARDINALITY"}:
        return 8, 1
    if case_id == "C1-PLACEMENT-GROUP-ANY":
        return 9, 1
    if family == "SetImageContent":
        return 2, 1
    if family == "SetVectorPathGeometry":
        return 3, 1
    if family == "EditRichText":
        return 4, 1
    if family in {"AddStroke", "SplitStrokes", "AddEraseMasks", "RemoveEraseMasks"}:
        return 5, 1
    if family == "SetConnectorContent":
        return 7, 1
    return 1, 1


def _content_for_kind(kind: int, case_id: str) -> dict[str, Any]:
    if kind == 2:
        return {"variant": 1, "value": {"resource_id": _stable_id(case_id, "resource"), "intrinsic_width": 320.0, "intrinsic_height": 200.0, "content_mode": 1, "width": 320.0, "height": 200.0}}
    if kind == 3:
        return {"variant": 2, "value": {"geometry": {"variant": 0, "value": {"segments": [{"p0": {"position": [0.0, 0.0], "radius": 1.0}, "p1": {"position": [1.0, 1.0], "radius": 1.0}, "control1": [0.25, 0.25], "control2": [0.75, 0.75]}]}}}}
    if kind == 4:
        paragraph_id = _stable_id(case_id, "paragraph")
        text = "λ-fixture" if "UTF8" in case_id else "fixture"
        return {"variant": 3, "value": {"document": {"paragraphs": [{"id": paragraph_id, "style": {"alignment": 1, "line_height": 1.0, "spacing_before": 0.0, "spacing_after": 0.0}, "runs": [{"text": text, "style": {"font_resource_id": paragraph_id, "font_size": 12.0, "weight": 400, "italic": False, "underline": False, "color": [0.0, 0.0, 0.0, 1.0]}}]}]}}}
    if kind == 5:
        return {"variant": 4, "value": {"stroke": {"deterministic_seed": 1, "brush_family_id": 1, "brush_version": 1, "color": [0.0, 0.0, 0.0, 1.0], "nominal_size": 1.0, "opacity": 1.0, "data_variant": 0, "data": [{"position": [0.0, 0.0], "pressure": 1.0, "tilt": [0.0, 0.0]}, {"position": [1.0, 1.0], "pressure": 1.0, "tilt": [0.0, 0.0]}]}}}
    if kind == 7:
        return {"variant": 6, "value": {"start": {"variant": 0, "value": {"point": [0.0, 0.0]}}, "end": {"variant": 0, "value": {"point": [10.0, 10.0]}}, "routing": 1}}
    if kind == 8:
        return {"variant": 7, "value": {"width": 20.0, "height": 20.0}}
    if kind == 9:
        return {"variant": 8, "value": {}}
    return {"variant": 0, "value": {"shape_kind": 1, "width": 32.0, "height": 24.0}}


def _object(case_id: str, family: str, role: str, ordinal: int = 1, parent_id: str | None = None) -> dict[str, Any]:
    kind, kind_version = _kind_for_family(family, case_id)
    content = _content_for_kind(kind, case_id)
    if "WRONG-CONTENT" in case_id or "INVALID-RECORD" in case_id:
        content = {"variant": 0, "value": {}}
    return {
        "id": _stable_id(case_id, role),
        "kind": kind,
        "kind_version": kind_version,
        "placement": {"parent_id": parent_id, "order_key": _order_key(case_id, ordinal)},
        "transform": [1.0, 0.0, 0.0, 1.0, 0.0, 0.0],
        "properties": [],
        "content": content,
        "erase_masks": [],
    }


def _initial_objects(case_id: str, family: str) -> list[dict[str, Any]]:
    objects: list[dict[str, Any]] = []
    target_needed = family not in {"InsertObjects", "RestoreObjects"}
    if case_id in {
        "C1-DELETE-MISSING-TARGET",
        "C1-SPLIT-SOURCE-MISSING",
        "C1-RESTORE-ELIGIBLE",
        "C1-RESTORE-ABSENT-REF",
        "C1-RESTORE-OPID-BEFORE-EXISTENCE",
        "C1-RESTORE-LOCAL-REPLAY-REMOTE",
        "C1-RESTORE-NO-TOMBSTONE",
    }:
        target_needed = False
    if case_id in {
        "C1-INSERT-EXISTING-ID",
        "C1-RESTORE-EXISTING-ID",
        "C1-RESTORE-EXISTING-ID-DIFFERENT",
        "C1-RESTORE-BATCH-EXISTING-ID",
        "C1-STROKE-EXISTING-ID",
        "C1-ERASE-ADD-EXISTING-MASK",
        "C1-SPLIT-REPLACEMENT-COLLISION",
    }:
        target_needed = True
    if target_needed:
        target = _object(case_id, family, "target")
        if case_id in {"C1-ERASE-ADD-EXISTING-MASK", "C1-ERASE-REMOVE-VALID", "C1-ERASE-REMOVE-DUPLICATE"}:
            target["erase_masks"] = [{"id": _stable_id(case_id, "mask"), "geometry": _geometry(case_id)}]
        objects.append(target)
    if case_id == "C1-RESTORE-SAME-PAYLOAD-NEW-OPID":
        objects.append(_object(case_id, family, "target"))
    if case_id in {"C1-DELETE-SUBTREE", "C1-DELETE-CASCADE"}:
        if case_id == "C1-DELETE-SUBTREE":
            objects.append(_object(case_id, family, "child", 2, _stable_id(case_id, "target")))
        else:
            connector = _object(case_id, "SetConnectorContent", "connector", 2)
            connector["content"] = {
                "variant": 6,
                "value": {
                    "start": {
                        "variant": 1,
                        "value": {
                            "target_object_id": _stable_id(case_id, "target"),
                            "anchor": {"variant": 0, "value": {"port_id": 1}},
                        },
                    },
                    "end": {"variant": 0, "value": {"point": [10.0, 10.0]}},
                    "routing": 1,
                },
            }
            objects.append(connector)
    if case_id == "C1-RESTORE-EXISTING-ID":
        objects.append(_object(case_id, family, "target"))
    if case_id == "C1-RESTORE-EXISTING-ID-DIFFERENT":
        target = _object(case_id, family, "target")
        target["transform"][0] = 2.0
        objects.append(target)
    if case_id == "C1-RESTORE-BATCH-EXISTING-ID":
        objects.append(_object(case_id, family, "target"))
    if case_id in {"C1-RESTORE-CONNECTOR-TARGET-ABSENT"}:
        # The connector's attached target is deliberately absent.
        pass
    if case_id in {"C1-PLACEMENT-INVALID-PARENT", "C1-PLACEMENT-NONPARENT"}:
        objects.append(_object(case_id, family, "unrelated-parent", 2))
    if case_id in {"C1-HIERARCHY-STICKY", "C1-PLACEMENT-STICKY-RICHTEXT", "C1-PLACEMENT-GROUP-ANY"}:
        parent = _object(case_id, family, "parent", 2)
        if case_id == "C1-PLACEMENT-GROUP-ANY":
            parent["kind"] = 9
            parent["kind_version"] = 1
        elif case_id == "C1-HIERARCHY-STICKY":
            parent["kind"] = 8
            parent["kind_version"] = 1
        elif case_id == "C1-PLACEMENT-STICKY-RICHTEXT":
            parent["kind"] = 8
            parent["kind_version"] = 1
        objects.append(parent)
    if case_id in {"C1-ERASE-REMOVE-WHOLE-REJECT"}:
        target = _object(case_id, family, "target")
        target["erase_masks"] = [
            {"id": _stable_id(case_id, "mask-a"), "geometry": _geometry(case_id)},
            {"id": _stable_id(case_id, "mask-b"), "geometry": _geometry(case_id)},
        ]
        objects = [target]
    if case_id in {"C1-CONNECTOR-VALID", "C1-CONNECTOR-ATTACHED-ENDPOINT", "C1-CONNECTOR-TARGET-CAPABILITY"}:
        endpoint = _object(case_id, "Shape", "endpoint-target", 2)
        if case_id == "C1-CONNECTOR-TARGET-CAPABILITY":
            endpoint["kind"] = 1
        objects.append(endpoint)
    return sorted({value["id"]: value for value in objects}.values(), key=lambda value: value["id"])


def _placement(case_id: str, target_id: str, parent_id: str | None = None) -> dict[str, Any]:
    return {"object_id": target_id, "placement": {"parent_id": parent_id, "order_key": _order_key(case_id, 1)}}


def _geometry(case_id: str) -> dict[str, Any]:
    if case_id in {"C1-GEOMETRY-N-1", "C1-GEOMETRY-N", "C1-GEOMETRY-BOUNDARY", "C1-GEOMETRY-LIMIT", "C1-GEOMETRY-OVERFLOW"}:
        units = {
            "C1-GEOMETRY-N-1": 1_999_999,
            "C1-GEOMETRY-N": 2_000_000,
            "C1-GEOMETRY-BOUNDARY": 2_000_000,
            "C1-GEOMETRY-LIMIT": 2_000_001,
            "C1-GEOMETRY-OVERFLOW": 2_000_001,
        }[case_id]
        return {"variant": 0, "value": {"geometryRecipe": {
            "carrier": "VectorPath",
            "move": 1,
            "line": units - 6,
            "quad": 1,
            "cubic": 1,
            "expectedUnits": units,
        }}}
    count = 3
    if "N-1" in case_id:
        count = 2
    elif case_id.endswith("GEOMETRY-N"):
        count = 3
    elif "OVERFLOW" in case_id:
        count = 5
    elif "LIMIT" in case_id:
        count = 4
    points = []
    for index in range(count):
        points.append({"p0": {"position": [float(index), 0.0], "radius": 1.0}, "p1": {"position": [float(index + 1), 1.0], "radius": 1.0}, "control1": [float(index) + 0.25, 0.25], "control2": [float(index) + 0.75, 0.75]})
    if "STRUCTURAL" in case_id:
        return {"variant": 0, "value": {"segments": []}}
    return {"variant": 0, "value": {"segments": points}}


def _payload(case_id: str, family: str, objects: list[dict[str, Any]]) -> tuple[int, dict[str, Any]]:
    target_id = _stable_id(case_id, "target")
    if family in {"InsertObjects", "RestoreObjects"}:
        if case_id in {"C1-INSERT-STAGED-PARENT", "C1-RESTORE-STAGED-PARENT-CHILD"}:
            parent = _object(case_id, family, "staged-parent", 1)
            parent["kind"] = 9
            parent["kind_version"] = 1
            parent["content"] = _content_for_kind(9, case_id)
            created = [parent, _object(case_id, family, "staged-child", 2, parent["id"])]
        elif case_id in {"C1-INSERT-STAGED-CONNECTOR", "C1-RESTORE-STAGED-CONNECTOR"}:
            endpoint = _object(case_id, "Shape", "staged-endpoint", 1)
            connector = _object(case_id, "SetConnectorContent", "staged-connector", 2)
            connector["content"] = {"variant": 6, "value": {"start": {"variant": 1, "value": {"target_object_id": endpoint["id"], "anchor": {"variant": 0, "value": {"port_id": 1}}}}, "end": {"variant": 0, "value": {"point": [10.0, 10.0]}}, "routing": 1}}
            created = [endpoint, connector]
        elif case_id == "C1-INSERT-HIERARCHY-CYCLE":
            first = _object(case_id, family, "cycle-a", 1)
            second = _object(case_id, family, "cycle-b", 2, first["id"])
            first["placement"]["parent_id"] = second["id"]
            created = [first, second]
        elif case_id == "C1-INSERT-STICKY-CARDINALITY":
            parent = _object(case_id, family, "sticky-parent", 1)
            children = []
            for role, ordinal in (("sticky-child-a", 2), ("sticky-child-b", 3)):
                child = _object(case_id, family, role, ordinal, parent["id"])
                child["kind"] = 4
                child["kind_version"] = 1
                child["content"] = _content_for_kind(4, case_id)
                children.append(child)
            created = [parent, *children]
        elif case_id == "C1-RESTORE-ABSENT-REF":
            created = [_object(case_id, family, "created", 1, _stable_id(case_id, "missing-parent"))]
        elif case_id == "C1-RESTORE-CONNECTOR-TARGET-ABSENT":
            connector = _object(case_id, "SetConnectorContent", "created", 1)
            connector["content"] = {"variant": 6, "value": {"start": {"variant": 1, "value": {"target_object_id": _stable_id(case_id, "missing-target"), "anchor": {"variant": 0, "value": {"port_id": 1}}}}, "end": {"variant": 0, "value": {"point": [10.0, 10.0]}}, "routing": 1}}
            created = [connector]
        else:
            created = [_object(case_id, family, "created", 1)]
            if "EXISTING-ID" in case_id or case_id == "C1-RESTORE-SAME-PAYLOAD-NEW-OPID":
                created[0]["id"] = target_id
        return (0 if family == "InsertObjects" else 2), {"objects": sorted(created, key=lambda value: value["id"])}
    if family == "DeleteObjects":
        ids = [target_id]
        if "DUPLICATE" in case_id:
            ids.append(target_id)
        if "SUBTREE" in case_id:
            ids.append(_stable_id(case_id, "child"))
        return 1, {"object_ids": ids}
    if family == "SetPlacements":
        parent_id = _stable_id(case_id, "parent") if "STAGED" in case_id or "PARENT" in case_id or case_id in {"C1-HIERARCHY-STICKY", "C1-PLACEMENT-STICKY-RICHTEXT", "C1-PLACEMENT-GROUP-ANY"} else None
        items = [_placement(case_id, target_id, parent_id)]
        if "CYCLE" in case_id:
            items[0]["placement"]["parent_id"] = target_id
        if case_id == "C1-PLACEMENT-INVALID-PARENT":
            items[0]["placement"]["parent_id"] = _stable_id(case_id, "missing-parent")
        if case_id == "C1-PLACEMENT-NONPARENT":
            items[0]["placement"]["parent_id"] = _stable_id(case_id, "unrelated-parent")
        if case_id == "C1-PLACEMENT-ORDERKEY":
            items[0]["placement"]["order_key"] = []
        return 3, {"items": items}
    if family == "SetTransforms":
        transform = [1.0, 0.0, 0.0, 1.0, 0.0, 0.0]
        if "NEGATIVE-ZERO" in case_id:
            transform[4] = -0.0
        if "NAN-INF" in case_id:
            transform = [F64_NAN, 0.0, 0.0, 1.0, F64_POS_INF, F64_NEG_INF]
        return 4, {"items": [{"object_id": target_id, "transform": transform}]}
    if family == "PatchProperties":
        field_id = 999999 if "FIELD-ID" in case_id else 1
        if case_id == "C1-PATCH-APPLICABILITY":
            field_id = 3
        action = "clear" if case_id == "C1-PATCH-PRESENCE-DEFAULT" else "set"
        patch: dict[str, Any] = {"object_id": target_id, "field_id": field_id, "action": action}
        if "PRESENCE-DEFAULT" not in case_id:
            patch["value"] = {"variant": 1, "value": "fixture"}
        if case_id == "C1-PATCH-APPLICABILITY":
            patch["value"] = {"variant": 1, "value": 0.5}
        if "BRANCH-TYPE" in case_id:
            patch["value"] = {"variant": 2, "value": 123}
        patches = [patch]
        if "DUPLICATE-FIELD" in case_id:
            patches.append(dict(patch))
        return 5, {"patches": patches}
    if family == "SetObjectSize":
        width, height = 32.0, 24.0
        if "NONFINITE" in case_id:
            width = F64_POS_INF
        if "HARD-LIMIT" in case_id:
            items = [
                {"object_id": _stable_id(case_id, f"size-{index}"), "width": width, "height": height}
                for index in range(65_536)
            ]
            return 6, {"items": sorted(items, key=lambda item: item["object_id"])}
        if "NONPOSITIVE" in case_id:
            width = -1.0
        return 6, {"items": [{"object_id": target_id, "width": width, "height": height}]}
    if family == "SetVectorPathGeometry":
        return 7, {"object_id": target_id, "geometry": _geometry(case_id)}
    if family == "SetImageContent":
        content: dict[str, Any] = {"resource_id": _stable_id(case_id, "resource"), "intrinsic_width": 320.0, "intrinsic_height": 200.0, "content_mode": 1, "width": 320.0, "height": 200.0}
        if "PRESENCE" in case_id:
            content = {}
        if "INTRINSIC" in case_id:
            content["intrinsic_width"], content["intrinsic_height"] = 640.0, 480.0
        if "CONTENTMODE" in case_id:
            content["content_mode"] = 2
        if "LOCAL-SIZE" in case_id:
            content["width"], content["height"] = 64.0, 48.0
        if "RUNTIME-RESOURCE" in case_id:
            content["resource_id"] = _stable_id(case_id, "runtime-resource")
        if "SOURCE-RECT" in case_id:
            content["source_rect"] = {"x": 0.5, "y": 0.0, "width": 0.75, "height": 1.0}
        return 8, {"object_id": target_id, "content": content}
    if family == "AddStroke":
        created = _object(case_id, family, "created")
        if case_id == "C1-STROKE-EXISTING-ID":
            created["id"] = target_id
        return 9, {"object": created}
    if family == "SplitStrokes":
        replacements = [_object(case_id, family, "replacement-1", 1), _object(case_id, family, "replacement-2", 2)]
        if "REPLACEMENT-COLLISION" in case_id:
            replacements[0]["id"] = target_id
        if "REPLACEMENT-STRUCTURAL" in case_id:
            replacements[0]["content"] = {"variant": 0, "value": {}}
        return 10, {"splits": [{"source_stroke_id": target_id, "replacements": sorted(replacements, key=lambda item: item["id"])}]}
    if family == "AddEraseMasks":
        geometry = {"variant": 0, "value": {"segments": [{"p0": {"position": [0.0, 0.0], "radius": 1.0}, "p1": {"position": [1.0, 1.0], "radius": 1.0}, "control1": [0.25, 0.25], "control2": [0.75, 0.75]}]}}
        if "GEOMETRY" in case_id:
            geometry = {"variant": 0, "value": {"segments": []}}
        mask = {"id": _stable_id(case_id, "mask"), "geometry": geometry}
        masks = [mask]
        if "UNIQUENESS" in case_id:
            masks.append(dict(mask))
        return 11, {"items": [{"object_id": target_id, "masks": masks}]}
    if family == "RemoveEraseMasks":
        mask_id = _stable_id(case_id, "mask")
        mask_ids = [mask_id, mask_id] if "DUPLICATE" in case_id else [mask_id]
        if "WHOLE-REJECT" in case_id:
            mask_ids = [_stable_id(case_id, "mask-a"), _stable_id(case_id, "mask-b"), _stable_id(case_id, "mask-missing")]
        return 12, {"items": [{"object_id": target_id, "mask_ids": sorted(mask_ids)}]}
    if family == "EditRichText":
        paragraph_id = _stable_id(case_id, "paragraph")
        step = {"kind": "InsertText", "paragraph_id": paragraph_id, "scalar_offset": 0, "text": "λ-fixture" if "UTF8" in case_id else "fixture", "style": {"font_resource_id": paragraph_id, "font_size": 12.0, "weight": 400, "italic": False, "underline": False, "color": [0.0, 0.0, 0.0, 1.0]}}
        if case_id == "C1-RICHTEXT-UTF8-STYLE":
            step["scalar_offset"] = 999
        if case_id == "C1-RICHTEXT-INVALID-STEP":
            step["paragraph_id"] = _stable_id(case_id, "missing-paragraph")
        return 13, {"object_id": target_id, "delta": {"delta_version": 1, "steps": [step]}}
    if family == "SetConnectorContent":
        endpoint = {"variant": 1, "value": {"target_object_id": _stable_id(case_id, "endpoint-target"), "anchor": {"variant": 0, "value": {"port_id": 1}}}}
        if "ANCHOR" in case_id:
            endpoint["value"]["anchor"]["value"]["port_id"] = 0
        invalid_end = {"variant": 99, "value": {}} if "INVALID-END" in case_id else {"variant": 0, "value": {"point": [10.0, 10.0]}}
        content = {"start": endpoint, "end": invalid_end, "routing": 99 if "ROUTING" in case_id else 1}
        return 14, {"object_id": target_id, "content": content}
    raise ValueError(f"unsupported operation family: {family}")


def _operation(case_id: str, family: str, objects: list[dict[str, Any]]) -> dict[str, Any]:
    variant, payload = _payload(case_id, family, objects)
    return {
        "id": _stable_id(case_id, "operation"),
        "document_id": _stable_id(case_id, "document"),
        "schema_version": 1,
        "payload_version": 1,
        "payload": {"variant": variant, "value": payload},
    }


def _prior_operations(case_id: str, operation: dict[str, Any]) -> list[dict[str, Any]]:
    if case_id in {"C1-RESTORE-LOCAL-REPLAY-REMOTE", "C1-RESTORE-SAME-PAYLOAD-NEW-OPID"}:
        return []
    if case_id not in {"C1-IDEMPOTENT-EQUIVALENT", "C1-ID-COLLISION", "C1-RESTORE-OPID-BEFORE-EXISTENCE"}:
        return []
    prior_id = operation["id"]
    prior_payload = operation["payload"]
    if case_id == "C1-ID-COLLISION":
        prior_payload = {"variant": 0, "value": {"objects": []}}
    prior = {"operation_id": prior_id, "document_id": operation["document_id"], "schema_version": 1, "payload_version": 1, "payload": prior_payload}
    return [prior]


def _input_record(case: dict[str, Any]) -> dict[str, Any]:
    case_id = case["id"]
    family = case["operationFamily"]
    objects = _initial_objects(case_id, family)
    operation = _operation(case_id, family, objects)
    variants: list[dict[str, str]] = []
    if "LOCAL-REPLAY-REMOTE" in case_id:
        variants = [{"name": "local"}, {"name": "replay"}, {"name": "remote"}]
    return {
        "format": FORMAT,
        "formatVersion": FORMAT_VERSION,
        "provenance": "DERIVED_GENERATED",
        "caseId": case_id,
        "operationFamily": family,
        "initialState": {"objects": objects, "priorOperations": _prior_operations(case_id, operation)},
        "operation": operation,
        "executionVariants": variants,
    }


def _source_info(cases_path: Path, expected_path: Path, suite_path: Path, raw_cases: bytes, raw_expected: bytes, raw_suite: bytes) -> dict[str, dict[str, str]]:
    return {
        "cases": {"path": cases_path.name, "sha256": _sha256(raw_cases)},
        "expected": {"path": expected_path.name, "sha256": _sha256(raw_expected)},
        "coreSuite": {"path": suite_path.name, "sha256": _sha256(raw_suite)},
    }


def _validate_sources(cases: list[dict[str, Any]], expected: list[dict[str, Any]], suite: dict[str, Any]) -> None:
    case_ids = [case.get("id") for case in cases]
    expected_ids = [record.get("caseId") for record in expected]
    if len(case_ids) != len(set(case_ids)) or any(not isinstance(case_id, str) or not case_id for case_id in case_ids):
        raise ValueError("cases must contain unique non-empty ids")
    if sorted(case_ids) != sorted(expected_ids):
        raise ValueError("cases and expected case ids must match exactly")
    suite_ids = suite.get("caseIds")
    if not isinstance(suite_ids, list) or sorted(case_ids) != sorted(suite_ids):
        raise ValueError("core suite membership must match accepted case ids")
    for case in cases:
        if case.get("inputRef") != f"generated/inputs/{case['id']}.json":
            raise ValueError(f"unexpected inputRef for {case['id']}")


def _ensure_safe_output(output: Path, cases_path: Path) -> Path:
    candidate = output.expanduser().resolve()
    authoring_root = cases_path.expanduser().resolve().parent
    if candidate == authoring_root or authoring_root in candidate.parents:
        raise ValueError("output target is inside the accepted authoring root")
    return candidate


def materialize(root: Path = ROOT, output: Path = DEFAULT_OUTPUT, *, cases_path: Path = DEFAULT_CASES, expected_path: Path = DEFAULT_EXPECTED, suite_path: Path = DEFAULT_SUITE) -> dict[str, Any]:
    del root
    cases_value, raw_cases = _read_json(cases_path)
    expected_value, raw_expected = _read_json(expected_path)
    suite_value, raw_suite = _read_json(suite_path)
    if not isinstance(cases_value, list) or not isinstance(expected_value, list) or not isinstance(suite_value, dict):
        raise ValueError("accepted sources have unexpected top-level shapes")
    cases = sorted(cases_value, key=lambda value: value["id"])
    expected_by_id = {record["caseId"]: record for record in expected_value}
    _validate_sources(cases, expected_value, suite_value)
    destination = _ensure_safe_output(output, cases_path)
    compiler_path = Path(__file__).resolve()
    compiler_sha = _sha256(compiler_path.read_bytes())
    source_files = _source_info(cases_path, expected_path, suite_path, raw_cases, raw_expected, raw_suite)

    rendered: list[tuple[str, bytes, bytes]] = []
    for case in cases:
        case_id = case["id"]
        input_value = _input_record(case)
        input_bytes = _canonical_bytes(input_value)
        expected_record = expected_by_id[case_id]
        provenance = {
            "format": "axiom-g1-04-c-provenance-v1",
            "formatVersion": 1,
            "provenance": "DERIVED_GENERATED",
            "caseId": case_id,
            "operationFamily": case["operationFamily"],
            "sourceCaseRef": f"authoring/cases.json#{case_id}",
            "sourceCaseRecordSha256": _sha256(_canonical_bytes(case)),
            "expectedRef": f"authoring/expected.json#{case_id}",
            "expectedRecordSha256": _sha256(_canonical_bytes(expected_record)),
            "caseAuthorityRuleRefs": list(case["authorityRuleRefs"]),
            "expectedAuthorityRuleRefs": list(expected_record["authorityRuleRefs"]),
            "compiler": {"identity": COMPILER_IDENTITY, "sourcePath": "verification/fixture-author/compile_g1_04_c.py", "sourceSha256": compiler_sha},
            "sourceFiles": source_files,
            "generatedInput": {"path": f"inputs/{case_id}.json", "sha256": _sha256(input_bytes), "bytes": len(input_bytes)},
        }
        rendered.append((case_id, input_bytes, _canonical_bytes(provenance)))

    entries = []
    for case_id, input_bytes, provenance_bytes in rendered:
        entries.append({"caseId": case_id, "input": {"path": f"inputs/{case_id}.json", "sha256": _sha256(input_bytes), "bytes": len(input_bytes)}, "provenance": {"path": f"provenance/{case_id}.json", "sha256": _sha256(provenance_bytes), "bytes": len(provenance_bytes)}})
    entries.sort(key=lambda value: value["caseId"])
    manifest = {
        "format": "axiom-g1-04-c-fixture-manifest-v1",
        "formatVersion": 1,
        "provenance": "DERIVED_GENERATED",
        "compiler": {"identity": COMPILER_IDENTITY, "sourceSha256": compiler_sha},
        "sourceFiles": source_files,
        "caseCount": len(cases),
        "blockingCaseCount": sum(1 for case in cases if case.get("blocking") is True),
        "entries": entries,
        "inventorySha256": _sha256(_canonical_bytes(entries)),
    }

    (destination / "inputs").mkdir(parents=True, exist_ok=True)
    (destination / "provenance").mkdir(parents=True, exist_ok=True)
    for case_id, input_bytes, provenance_bytes in rendered:
        (destination / "inputs" / f"{case_id}.json").write_bytes(input_bytes)
        (destination / "provenance" / f"{case_id}.json").write_bytes(provenance_bytes)
    (destination / "manifest.json").write_bytes(_canonical_bytes(manifest))
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Compile the accepted C1 corpus into deterministic derived fixtures.")
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--cases", type=Path, default=DEFAULT_CASES)
    parser.add_argument("--expected", type=Path, default=DEFAULT_EXPECTED)
    parser.add_argument("--suite", type=Path, default=DEFAULT_SUITE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args(argv)
    try:
        manifest = materialize(args.root, args.output, cases_path=args.cases, expected_path=args.expected, suite_path=args.suite)
    except Exception as exc:  # pragma: no cover - exercised through the CLI boundary
        print(json.dumps({"ok": False, "error": str(exc)}, sort_keys=True), file=sys.stderr)
        return 2
    print(json.dumps({"ok": True, "caseCount": manifest["caseCount"], "output": str(args.output)}, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
