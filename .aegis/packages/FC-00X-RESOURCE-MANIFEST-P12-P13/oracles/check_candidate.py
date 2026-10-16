#!/usr/bin/env python3
"""Read-only P12/P13 candidate consistency oracle; never imports product code."""
from pathlib import Path
import hashlib, json, re, struct, sys

ROOT = Path(__file__).resolve().parents[1]
errors = []
def read(rel):
    p = ROOT / rel
    if not p.exists():
        errors.append(f"missing:{rel}")
        return ""
    return p.read_text(encoding="utf-8")
def sha(rel):
    return hashlib.sha256((ROOT / rel).read_bytes()).hexdigest()
proto = read("drafts/resource_manifest_v1.proto.txt")
operation = read("resource_manifest_operation.v1.md")
snapshot = read("snapshot_binding.v1.md")
digest = read("digest_projection.v1.md")
matrix = read("candidate_binding_oracle_matrix.v1.md")
cap = read("capability_registry.v1.yaml")
registry = read("operation_registry.resource_manifest.v1.yaml")
kind = read("resource_kind_registry.v1.yaml")
for needle in ("bytes lineage_token = 6", "bytes expected_lineage_token = 4", "repeated ResourceManifestEntry upserts = 1", "repeated Id128 removes = 2", "repeated ResourceExpectation expectations = 3", "repeated ReactivateResource reactivations = 4", "uint64 expected_manifest_revision = 5"):
    if needle not in proto: errors.append(f"proto:{needle}")
for needle in ("expected_removal_operation_id=2", "expected_preimage=3", "expected_lineage_token=4", "materialized in"):
    if needle not in operation: errors.append(f"operation:{needle}")
if "same-ID coexistence allowed" not in proto: errors.append("proto:tombstone-coexistence-rule")
for field in range(4, 11):
    if f"| {field} |" not in snapshot: errors.append(f"snapshot:field-{field}")
if "schema_version=3" not in snapshot and "schema_version | 3" not in snapshot: errors.append("snapshot:schema3")
for needle in ("u32(28)", "manifest_presence", "manifest_revision", "tombstone_count", "shape2-triangle-v1", "document_revision, recovery_frontier, checkpoint_digest"):
    if needle not in digest: errors.append(f"digest:{needle}")
if "RM-DIGEST-REFERENCE" not in matrix: errors.append("matrix:reference-digest-row")
for needle in ("legacy-no-manifest-v1", "resource-manifest-v1", "shape2-triangle-v1", "numeric_value: 1"):
    if needle not in cap: errors.append(f"capability:{needle}")
for needle in ("profile_id: axiom.operation-registry.resource-manifest-v1", "imported_operations: 1..15", "id: 16, tag: 16", "reject_unknown_operation_16_before_mutation"):
    if needle not in registry: errors.append(f"operation-registry:{needle}")
for needle in ("metadata_schemas:", "max_entries: 65535", "max_tombstones: 65535", "max_lineage_token_bytes: 32"):
    if needle not in kind: errors.append(f"kind-registry:{needle}")
for review in ("independent-control-review-v0.1.json", "independent-control-review-v0.2.json", "independent-control-review-v0.3.json"):
    if not (ROOT / review).exists(): errors.append(f"history:{review}")

# Independent reference encoder for the candidate projection. It uses opaque fixture bytes
# for ObjectRecord/manifest records and never imports the product serializer.
def u32(value): return struct.pack("<I", value)
def u64(value): return struct.pack("<Q", value)
def varint(value):
    if value < 0: raise ValueError("negative varint")
    out = bytearray()
    while value >= 0x80:
        out.append((value & 0x7f) | 0x80)
        value >>= 7
    out.append(value)
    return bytes(out)
def capability_bytes(ids):
    out = bytearray()
    for item in sorted(ids):
        raw = item.encode("ascii")
        out += b"\x0a" + varint(len(raw)) + raw
    return bytes(out)
def projection(*, schema, capabilities=b"", presence=0, objects=(), manifest_revision=0, entries=(), tombstones=(), runtime_revision=None):
    domain = b"axiom.semantic.projection.v1"
    out = bytearray(u32(len(domain)) + domain)
    out += bytes(16) + u32(schema) + u32(len(capabilities)) + capabilities + bytes([presence])
    ordered_objects = sorted(objects)
    out += u64(len(ordered_objects))
    for blob in ordered_objects: out += u32(len(blob)) + blob
    out += u32(0 if not presence else 1) + u64(manifest_revision)
    ordered_entries = sorted(entries)
    out += u64(len(ordered_entries))
    for blob in ordered_entries: out += u32(len(blob)) + blob
    ordered_tombstones = sorted(tombstones)
    out += u64(len(ordered_tombstones))
    for blob in ordered_tombstones: out += u32(len(blob)) + blob
    # runtime_revision is deliberately accepted and ignored: it is checkpoint metadata.
    _ = runtime_revision
    return bytes(out)
absent = projection(schema=2)
explicit = projection(schema=3, capabilities=capability_bytes(["resource-manifest-v1"]), presence=1)
revision_a = projection(schema=3, capabilities=capability_bytes(["resource-manifest-v1"]), presence=1, manifest_revision=0)
revision_b = projection(schema=3, capabilities=capability_bytes(["resource-manifest-v1"]), presence=1, manifest_revision=1)
tombstone = projection(schema=3, capabilities=capability_bytes(["resource-manifest-v1"]), presence=1, tombstones=(b"tombstone",))
runtime_a = projection(schema=3, capabilities=capability_bytes(["resource-manifest-v1"]), presence=1, runtime_revision=1)
runtime_b = projection(schema=3, capabilities=capability_bytes(["resource-manifest-v1"]), presence=1, runtime_revision=999)
order_a = projection(schema=3, capabilities=capability_bytes(["resource-manifest-v1", "shape2-triangle-v1"]), presence=1, objects=(b"b", b"a"), entries=(b"entry-b", b"entry-a"), tombstones=(b"tomb-b", b"tomb-a"))
order_b = projection(schema=3, capabilities=capability_bytes(["shape2-triangle-v1", "resource-manifest-v1"]), presence=1, objects=(b"a", b"b"), entries=(b"entry-a", b"entry-b"), tombstones=(b"tomb-a", b"tomb-b"))
reference = {
    "domain_prefix_length": len(b"axiom.semantic.projection.v1"),
    "absent_sha256": hashlib.sha256(absent).hexdigest(),
    "explicit_empty_sha256": hashlib.sha256(explicit).hexdigest(),
    "revision_a_sha256": hashlib.sha256(revision_a).hexdigest(),
    "revision_b_sha256": hashlib.sha256(revision_b).hexdigest(),
    "tombstone_sha256": hashlib.sha256(tombstone).hexdigest(),
    "runtime_a_sha256": hashlib.sha256(runtime_a).hexdigest(),
    "runtime_b_sha256": hashlib.sha256(runtime_b).hexdigest(),
    "order_a_sha256": hashlib.sha256(order_a).hexdigest(),
    "order_b_sha256": hashlib.sha256(order_b).hexdigest(),
}
if reference["domain_prefix_length"] != 28: errors.append("digest:domain-length")
if reference["absent_sha256"] == reference["explicit_empty_sha256"]: errors.append("digest:presence-not-distinct")
if reference["revision_a_sha256"] == reference["revision_b_sha256"]: errors.append("digest:manifest-revision-not-distinct")
if reference["runtime_a_sha256"] != reference["runtime_b_sha256"]: errors.append("digest:runtime-revision-leaked")
if reference["revision_a_sha256"] == reference["tombstone_sha256"]: errors.append("digest:tombstone-not-distinct")
if reference["order_a_sha256"] != reference["order_b_sha256"]: errors.append("digest:ordering-not-canonical")
result = {"oracle":"FC-00X-RM-CANDIDATE-V0.4", "status":"PASS" if not errors else "FAIL", "errors":errors, "reference_vectors":reference, "checked_files": {"proto":sha("drafts/resource_manifest_v1.proto.txt"), "operation":sha("resource_manifest_operation.v1.md"), "snapshot":sha("snapshot_binding.v1.md"), "digest":sha("digest_projection.v1.md"), "capability":sha("capability_registry.v1.yaml"), "operation_registry":sha("operation_registry.resource_manifest.v1.yaml"), "resource_kind":sha("resource_kind_registry.v1.yaml")}}
print(json.dumps(result, indent=2, sort_keys=True))
sys.exit(0 if not errors else 1)
