#!/usr/bin/env python3
"""Validate the approved geometry accounting machine projection without PyYAML."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[2]
PROJECTION = ROOT / "schema/axiom/v1/canonical/geometry_accounting_v1.yaml"
LIMITS = ROOT / "schema/axiom/v1/canonical/protocol_hard_limits_v1.yaml"


def _yaml(path: Path):
    script = r'''
      require 'yaml'
      require 'json'
      def reject_duplicates(node, location = [])
        case node
        when Psych::Nodes::Document
          node.children.each { |child| reject_duplicates(child, location) }
        when Psych::Nodes::Mapping
          seen = {}
          node.children.each_slice(2) do |key, value|
            name = key.respond_to?(:value) ? key.value.to_s : '<complex-key>'
            raise "duplicate mapping key: #{(location + [name]).join('.')}" if seen[name]
            seen[name] = true
            reject_duplicates(value, location + [name])
          end
        when Psych::Nodes::Sequence
          node.children.each { |child| reject_duplicates(child, location) }
        end
      end
      source = File.read(ARGV.fetch(0))
      reject_duplicates(Psych.parse(source))
      puts JSON.generate(YAML.safe_load(source, aliases: false))
    '''
    result = subprocess.run(["ruby", "-e", script, str(path)], capture_output=True, text=True)
    if result.returncode:
        raise ValueError(result.stderr.strip() or "invalid YAML")
    return json.loads(result.stdout)


def validate(root: Path = ROOT) -> list[str]:
    projection = _yaml(root / PROJECTION)
    limits = _yaml(root / LIMITS)
    failures: list[str] = []
    if projection.get("schema") != "auditoryworks.axiom.v1.geometry-accounting": failures.append("schema identifier mismatch")
    if projection.get("version") != 1 or projection.get("status") != "frozen": failures.append("projection version/status mismatch")
    source = projection.get("source_authority", {})
    if source.get("notion_page_id") != "3c94c57a-590c-81e1-aea7-eae7ea8a8c88": failures.append("human authority reference mismatch")
    if projection.get("source_projection_contract", {}).get("notion_page_id") != "3c94c57a-590c-8182-b677-df77433c5706": failures.append("projection authority reference mismatch")
    limit = projection.get("limit", {})
    if limit.get("source_path") != "schema/axiom/v1/canonical/protocol_hard_limits_v1.yaml": failures.append("limit source path mismatch")
    if limit.get("source_key") != "limits.geometry_point_like_elements_per_operation_aggregate": failures.append("limit source key mismatch")
    if limit.get("value") != limits.get("limits", {}).get("geometry_point_like_elements_per_operation_aggregate"): failures.append("limit value drift")
    carriers = projection.get("carriers", {})
    if len(carriers) != len(set(carriers)): failures.append("duplicate carrier key")
    if any(not isinstance(value, int) or value < 0 for value in carriers.values()): failures.append("carrier weights must be non-negative integers")
    required = {"MoveTo.point": 1, "LineTo.end": 1, "QuadTo.control": 1, "QuadTo.end": 1,
                "CubicTo.control1": 1, "CubicTo.control2": 1, "CubicTo.end": 1, "ClosePath": 0,
                "StrokeSample.position": 1, "DabInstance.center": 1, "DabInstance.size": 1,
                "DabInstance.rotation": 1, "DabInstance.opacity": 0, "EraseKnot.position": 1,
                "EraseKnot.radius": 1, "EraseCubicSegment.control1": 1, "EraseCubicSegment.control2": 1}
    for key, expected in required.items():
        if carriers.get(key) != expected: failures.append(f"carrier drift: {key}")
    operations = projection.get("operations", {})
    required_ops = {"InsertObjects", "DeleteObjects", "RestoreObjects", "SetPlacements", "SetTransforms", "PatchProperties", "SetObjectSize", "SetVectorPathGeometry", "SetImageContent", "AddStroke", "SplitStrokes", "AddEraseMasks", "RemoveEraseMasks", "EditRichText", "SetConnectorContent"}
    if set(operations) != required_ops: failures.append("operation mapping inventory mismatch")
    if projection.get("ownership", {}).get("normative_exact_validation") != "A3": failures.append("A3 ownership mismatch")
    if projection.get("ownership", {}).get("early_raw_wire_rejection_when_provable") != "A2": failures.append("A2 ownership mismatch")
    return failures


def main() -> int:
    failures = validate()
    print(json.dumps({"failures": failures}, indent=2, sort_keys=True))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
