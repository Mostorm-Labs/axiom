#!/usr/bin/env python3
"""Validate the narrow publication contract for the G1-04 authority mirror."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[2]
MATRIX = Path(
    "docs/notion/authority/04-semantic-schema/04-reference-idl/"
    "richtext-stroke-semantic-wire-reconciliation-matrix-v0.1.md"
)
MANIFEST = Path("docs/notion/manifest.yaml")
INDEX = Path("docs/notion/authority/04-semantic-schema/README.md")
PAGE_ID = "3c94c57a-590c-8172-a7ad-ec6bb5f7bd91"
REQUIRED_MIRRORS = {
    "docs/notion/authority/04-semantic-schema/00-overview/v1-release-candidate-final-gate-v0.1.md",
    "docs/notion/authority/04-semantic-schema/04-reference-idl/reference-idl-codec-mapping-v0.1.md",
    "docs/notion/authority/04-semantic-schema/04-reference-idl/leaf-schema-reconciliation-v0.1.md",
    "docs/notion/authority/04-semantic-schema/04-reference-idl/generated-proto-canonical-codec-freeze-v0.1.md",
    "docs/notion/authority/04-semantic-schema/05-leaf-schemas/richtext-wire-schema-v0.1.md",
    "docs/notion/authority/04-semantic-schema/05-leaf-schemas/richtext-font-v1-release-v0.1.md",
    "docs/notion/authority/04-semantic-schema/05-leaf-schemas/brush-stroke-wire-schema-v0.1.md",
    "docs/notion/authority/04-semantic-schema/05-leaf-schemas/brush-family-registry-v1-release-v0.1.md",
    "docs/notion/authority/04-semantic-schema/05-leaf-schemas/pressure-tilt-v1-release-v0.1.md",
    str(MATRIX),
}
DEFECT_IDS = [*(f"RT-D0{i}" for i in range(1, 7)), *(f"ST-D0{i}" for i in range(1, 6))]


def _load_yaml(path: Path) -> object:
    # Ruby ships on the supported macOS and hosted Ubuntu toolchains while PyYAML
    # is intentionally not a repository dependency. Keep parsing delegated to a
    # real YAML parser rather than approximating YAML with line matching.
    script = """
        require 'yaml'
        require 'json'
        require 'date'

        def reject_duplicate_keys(node, location = [])
          case node
          when Psych::Nodes::Document
            node.children.each { |child| reject_duplicate_keys(child, location) }
          when Psych::Nodes::Mapping
            seen = {}
            node.children.each_slice(2) do |key, value|
              name = key.respond_to?(:value) ? key.value.to_s : '<complex-key>'
              raise "duplicate mapping key: #{(location + [name]).join('.')}" if seen[name]
              seen[name] = true
              reject_duplicate_keys(value, location + [name])
            end
          when Psych::Nodes::Sequence
            node.children.each { |child| reject_duplicate_keys(child, location) }
          end
        end

        source = File.read(ARGV.fetch(0))
        reject_duplicate_keys(Psych.parse(source))
        puts JSON.generate(YAML.safe_load(source, aliases: false,
          permitted_classes: [Date, Time, Symbol]))
    """
    result = subprocess.run(
        ["ruby", "-e", script, str(path)], capture_output=True, text=True
    )
    if result.returncode:
        raise ValueError(result.stderr.strip() or "manifest is not valid YAML")
    return json.loads(result.stdout)


def _walk_paths(value: object) -> list[str]:
    paths: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            if key == "path" and isinstance(child, str):
                paths.append(child)
            paths.extend(_walk_paths(child))
    elif isinstance(value, list):
        for child in value:
            paths.extend(_walk_paths(child))
    return paths


def validate(root: Path = ROOT) -> list[str]:
    failures: list[str] = []
    matrix = root / MATRIX
    manifest = root / MANIFEST
    index = root / INDEX
    if not matrix.is_file():
        failures.append(f"missing reconciliation matrix: {MATRIX}")
        return failures
    matrix_text = matrix.read_text(encoding="utf-8")
    for token in ("MACHINE_PROJECTION_DRIFT_CONFIRMED", "Architecture changed: `false`", "current-reconciliation-authority"):
        if token not in matrix_text:
            failures.append(f"matrix missing required metadata: {token}")
    for defect_id in DEFECT_IDS:
        if defect_id not in matrix_text:
            failures.append(f"matrix missing defect ID: {defect_id}")
    try:
        parsed = _load_yaml(manifest)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        failures.append(f"manifest parse failure: {error}")
        return failures
    if not isinstance(parsed, dict):
        failures.append("manifest root must be a mapping")
        return failures
    if parsed.get("repository_snapshot", {}).get("normative_only_when_listed_current") is not True:
        failures.append("normative_only_when_listed_current must remain true")
    paths = set(_walk_paths(parsed.get("current_authority", {})))
    missing_required = sorted(REQUIRED_MIRRORS - paths)
    failures.extend(f"manifest missing current mirror path: {path}" for path in missing_required)
    semantic_authority = parsed.get("current_authority", {}).get("semantic_schema_04", {})
    matrix_entry = semantic_authority.get("richtext_stroke_reconciliation")
    if not isinstance(matrix_entry, dict):
        failures.append("manifest missing current_authority.richtext_stroke_reconciliation")
    else:
        if matrix_entry.get("version") != "v0.1":
            failures.append("reconciliation matrix version must be v0.1")
        if matrix_entry.get("status") != "current-reconciliation-authority":
            failures.append("reconciliation matrix status must be current-reconciliation-authority")
        if matrix_entry.get("notion_page_id") != PAGE_ID:
            failures.append("reconciliation matrix Notion page ID mismatch")
        if matrix_entry.get("path") != str(MATRIX):
            failures.append("reconciliation matrix path mismatch")
    if not index.is_file():
        failures.append(f"missing 04 authority index: {INDEX}")
    elif MATRIX.name not in index.read_text(encoding="utf-8"):
        failures.append("04 authority index does not list reconciliation matrix")
    return failures


def main() -> int:
    failures = validate()
    if failures:
        print("\n".join(failures))
        return 1
    print("G1-04 authority publication contract: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
