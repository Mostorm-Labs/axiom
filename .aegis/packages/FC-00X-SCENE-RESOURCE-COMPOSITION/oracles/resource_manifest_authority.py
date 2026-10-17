#!/usr/bin/env python3
"""P31 authority-binding oracle for the Scene/resource/composition package.

The upstream P12/P13 Authority may be accepted while the product implementation
at the bound implementation revision is explicit. The oracle distinguishes exact binding from source absence:
missing exact binding is BLOCKED_AUTHORITY; an exact binding with no typed
product source is BLOCKED_IMPLEMENTATION.  The oracle is read-only.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

EXPECTED_REVISION = "057464f9fab2db4b67c032134cce3f5f417a64e0"
EXPECTED_AUTHORITY_ID = "fc00x-resource-manifest-p12-p13-v0.4"
EXPECTED_IMPLEMENTATION_REVISION = "4bef9d6e222d156a79a8358f7d88908a48e39f67"
EXPECTED_MATERIALIZED_REF = "601304425870ceb5715ae34a6da3b6ef1dfdd7e5"
EXPECTED_IMPLEMENTATION_BRANCH = "codex/fc00x-resource-manifest-implementation"
EXPECTED_PACKAGE_REF = (
    "repo:.aegis/packages/FC-00X-RESOURCE-MANIFEST-P12-P13/package.json@"
    + EXPECTED_REVISION
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--expect", choices=("baseline", "bound"), required=True)
    args = parser.parse_args()
    root = args.root.resolve()

    lock_path = root / ".aegis/packages/FC-00X-SCENE-RESOURCE-COMPOSITION/authority.lock.json"
    try:
        lock = json.loads(lock_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"BLOCKED_AUTHORITY: cannot read package Authority lock: {exc}")
        return 2

    binding = lock.get("accepted_upstream_authority", {})
    implementation = binding.get("typed_product_implementation", {})
    implementation_ok = (
        implementation.get("implementation_revision") == EXPECTED_IMPLEMENTATION_REVISION
        and implementation.get("materialized_ref") == EXPECTED_MATERIALIZED_REF
        and implementation.get("branch") == EXPECTED_IMPLEMENTATION_BRANCH
    )
    binding_ok = (
        binding.get("id") == EXPECTED_AUTHORITY_ID
        and binding.get("status") == "Current"
        and binding.get("accepted_revision") == EXPECTED_REVISION
        and binding.get("package_ref") == EXPECTED_PACKAGE_REF
    )
    if not binding_ok:
        print(
            "BLOCKED_AUTHORITY: exact accepted P12/P13 binding is missing or mismatched",
            binding,
        )
        return 2

    if not implementation_ok:
        print("BLOCKED_IMPLEMENTATION: typed ResourceManifest implementation binding is missing or mismatched", implementation)
        return 2

    candidates = [
        root / "runtime/semantic/include/canvas/semantic/resource_manifest.hpp",
        root / "runtime/semantic/include/canvas/semantic/snapshot.hpp",
        root / "schema/axiom/v1/proto/auditoryworks/axiom/v1/snapshot.proto",
    ]
    markers = []
    for path in candidates:
        try:
            text = path.read_text(encoding="utf-8")
        except OSError:
            text = ""
        markers.append(
            (
                str(path.relative_to(root)),
                "ResourceManifest" in text or "resource_manifest" in text,
            )
        )

    if args.expect == "baseline":
        if not all(value for _, value in markers):
            print(
                "BLOCKED_IMPLEMENTATION: accepted P12/P13 Authority is bound, "
                "but no typed ResourceManifest product source is present at the anchor",
                markers,
            )
            return 2
        print("VERIFICATION_DESIGN_DEFECT: typed manifest source already present at baseline", markers)
        return 1

    if not all(value for _, value in markers):
        print("FAIL: bound manifest source is incomplete", markers)
        return 1
    print("PASS: exact P12/P13 Authority binding, typed implementation revision and typed manifest source are present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
