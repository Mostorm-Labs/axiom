#!/usr/bin/env python3
"""Fail-closed, deterministic accounting for GT-G1-04-A authority coverage."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MATRIX = ROOT / "docs/planning/GT_G1_04_A_AUTHORITY_COVERAGE_MATRIX.md"
GEOMETRY_PROJECTION = ROOT / "schema/axiom/v1/canonical/geometry_accounting_v1.yaml"
HEADERS = [
    "Authority Rule ID", "Source Authority", "Normative Rule", "Owner",
    "Carrier", "Implementation location", "Test / Oracle", "Status",
]
INVALID_A_STATUSES = {"MISSING", "WRONG_ORACLE", "UNOWNED", "BLOCKED_CARRIER"}
VALID_STATUSES = {"COVERED", "DEFERRED_B", "DEFERRED_C", *INVALID_A_STATUSES}


def _cells(line: str) -> list[str]:
    return [cell.strip() for cell in line.strip().strip("|").split("|")]


def parse_matrix(text: str) -> list[dict[str, str]]:
    marker = "## P34 Review Finding Lineage"
    normative = text.split(marker, 1)[0]
    lines = normative.splitlines()
    header_index = next((i for i, line in enumerate(lines) if _cells(line) == HEADERS), None)
    if header_index is None or header_index + 1 >= len(lines):
        raise ValueError("normative coverage table header is missing")
    rows: list[dict[str, str]] = []
    for line in lines[header_index + 2:]:
        if not line.startswith("|"):
            break
        cells = _cells(line)
        if len(cells) != len(HEADERS):
            raise ValueError(f"malformed normative table row: {line}")
        rows.append(dict(zip(HEADERS, cells)))
    if not rows:
        raise ValueError("normative coverage table has no rows")
    return rows


def summarize(rows: list[dict[str, str]]) -> dict[str, object]:
    memberships = {owner: 0 for owner in ("A0", "A1", "A2", "A3")}
    flagged = {"missingRows": [], "wrongOracleRows": [], "unownedRows": [], "blockedCarrierRows": []}
    covered_a_rows = 0
    deferred_b_rows = 0
    deferred_c_rows = 0
    errors: list[str] = []

    for row in rows:
        rule_id = row["Authority Rule ID"]
        owners = [owner.strip() for owner in row["Owner"].split("/")]
        status = row["Status"]
        if status not in VALID_STATUSES:
            errors.append(f"{rule_id}: unsupported status {status}")
        a_owners = [owner for owner in owners if owner in memberships]
        for owner in a_owners:
            memberships[owner] += 1
        if status == "DEFERRED_B":
            if owners != ["B"]:
                errors.append(f"{rule_id}: DEFERRED_B requires exactly owner B")
            deferred_b_rows += 1
        elif status == "DEFERRED_C":
            if owners != ["C"]:
                errors.append(f"{rule_id}: DEFERRED_C requires exactly owner C")
            deferred_c_rows += 1
        elif a_owners and status == "COVERED":
            covered_a_rows += 1
        elif a_owners and status in INVALID_A_STATUSES:
            key = {
                "MISSING": "missingRows", "WRONG_ORACLE": "wrongOracleRows",
                "UNOWNED": "unownedRows", "BLOCKED_CARRIER": "blockedCarrierRows",
            }[status]
            flagged[key].append(rule_id)
            errors.append(f"{rule_id}: A-owned row may not be {status}")
        elif a_owners:
            errors.append(f"{rule_id}: A-owned row must be COVERED")

    return {
        "normativeRuleRows": len(rows),
        "ownerMemberships": memberships,
        "coveredARows": covered_a_rows,
        "deferredBRows": deferred_b_rows,
        "deferredCRows": deferred_c_rows,
        **flagged,
        "errors": errors,
    }


def validate_geometry_projection() -> list[str]:
    text = GEOMETRY_PROJECTION.read_text(encoding="utf-8")
    required = (
        "schema: auditoryworks.axiom.v1.geometry-accounting",
        "notion_page_id: 3c94c57a-590c-81e1-aea7-eae7ea8a8c88",
        "notion_page_id: 3c94c57a-590c-8182-b677-df77433c5706",
        "source_key: limits.geometry_point_like_elements_per_operation_aggregate",
        "value: 2000000",
        '"MoveTo.point": 1',
        '"LineTo.end": 1',
        '"QuadTo.control": 1',
        '"CubicTo.control1": 1',
        "ClosePath: 0",
        '"StrokeSample.position": 1',
        '"DabInstance.center": 1',
        '"DabInstance.opacity": 0',
        '"EraseCubicSegment.control1": 1',
        "normative_exact_validation: A3",
        "early_raw_wire_rejection_when_provable: A2",
    )
    return [f"geometry projection missing: {token}" for token in required if token not in text]


def main(argv: list[str]) -> int:
    matrix = Path(argv[1]) if len(argv) == 2 else DEFAULT_MATRIX
    try:
        result = summarize(parse_matrix(matrix.read_text(encoding="utf-8")))
        result["projectionErrors"] = validate_geometry_projection()
        result["errors"].extend(result["projectionErrors"])
    except (OSError, ValueError) as error:
        print(json.dumps({"error": str(error)}, sort_keys=True))
        return 1
    print(json.dumps({key: value for key, value in result.items() if key != "errors"}, sort_keys=True))
    if result["errors"]:
        print("\n".join(result["errors"]), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
