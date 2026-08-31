import test from "node:test";
import assert from "node:assert/strict";
import { coordinateCase } from "../dist/coordinator.js";

function base({ openPolicy = false } = {}) {
  const id = "CASE";
  return {
    caseIntent: { format: "axiom-g1-04-c-case-v1", formatVersion: 1, provenance: "AUTHORITY_MANUAL", id, operationFamily: "x", authorityRuleRefs: [], inputRef: "input", expectedRef: "expected", blocking: true },
    expected: { format: "axiom-g1-04-c-expected-v1", formatVersion: 1, provenance: "AUTHORITY_MANUAL", caseId: id, authorityRuleRefs: [], mutationExpected: false, disposition: "PLAN_READY", terminalPhase: "PREPARE", ...(openPolicy ? { openPolicy: true } : {}) },
    reference: { format: "axiom-g1-04-c-observation-v1", formatVersion: 1, provenance: "IMPLEMENTATION_OBSERVATION", caseId: id, provider: "reference", observedDisposition: "PLAN_READY", observedTerminalPhase: "PREPARE", beforeProjection: { x: 1 }, afterProjection: { x: 1 } },
    indexed: { format: "axiom-g1-04-c-observation-v1", formatVersion: 1, provenance: "IMPLEMENTATION_OBSERVATION", caseId: id, provider: "indexed", observedDisposition: "PLAN_READY", observedTerminalPhase: "PREPARE", beforeProjection: { x: 1 }, afterProjection: { x: 1 } },
    referenceRef: "ref", indexedRef: "idx", openAuthorityDecision: "UNRESOLVED"
  };
}

function assertResult(r, status, required = []) { assert.equal(r.status, status); for (const d of required) assert.ok(r.diagnostics?.includes(d), `${d} missing`); }

test("C4-T01 both providers match manual expected", () => assertResult(coordinateCase(base()), "PASS"));
test("C4-T02 provider agreement cannot override manual golden", () => { const x = base(); x.reference.observedDisposition = x.indexed.observedDisposition = "REJECTED"; const r = coordinateCase(x); assertResult(r, "FAIL", ["REFERENCE_GOLDEN_MISMATCH", "INDEXED_GOLDEN_MISMATCH"]); assert.ok(!r.diagnostics?.includes("PROVIDER_DIVERGENCE")); });
test("C4-T03 one provider differs", () => { const x = base(); x.indexed.observedDisposition = "REJECTED"; assertResult(coordinateCase(x), "FAIL", ["INDEXED_GOLDEN_MISMATCH", "PROVIDER_DIVERGENCE"]); });
test("C4-T04 reference mutation fails independently", () => { const x = base(); x.reference.afterProjection = { x: 2 }; assertResult(coordinateCase(x), "FAIL", ["REFERENCE_MUTATION"]); });
test("C4-T05 indexed mutation fails independently", () => { const x = base(); x.indexed.afterProjection = { x: 2 }; assertResult(coordinateCase(x), "FAIL", ["INDEXED_MUTATION"]); });
test("C4-T06 invalid provenance precedes semantic comparison", () => { const x = base(); x.expected.provenance = "IMPLEMENTATION_OBSERVATION"; x.reference.observedDisposition = "REJECTED"; const r = coordinateCase(x); assertResult(r, "FAIL", ["EXPECTED_PROVENANCE_INVALID"]); assert.ok(!r.diagnostics?.includes("REFERENCE_GOLDEN_MISMATCH")); });
test("C4-T07 current OPEN is observation only", () => { const x = base({ openPolicy: true }); x.openAuthorityDecision = "CURRENT_OPEN"; assertResult(coordinateCase(x), "OBSERVATION_ONLY", ["OPEN_POLICY_OBSERVATION_ONLY"]); });
test("C4-T08 stale OPEN fails closed", () => { const x = base({ openPolicy: true }); x.openAuthorityDecision = "CURRENT_CLOSED"; assertResult(coordinateCase(x), "FAIL", ["OPEN_POLICY_STALE_CLOSED"]); });
test("C4-T09 unresolved OPEN fails closed", () => assertResult(coordinateCase(base({ openPolicy: true })), "FAIL", ["OPEN_AUTHORITY_UNRESOLVED"]));
test("C4-T10 OPEN does not waive no mutation", () => { const x = base({ openPolicy: true }); x.openAuthorityDecision = "CURRENT_OPEN"; x.reference.afterProjection = { x: 2 }; const r = coordinateCase(x); assertResult(r, "FAIL", ["REFERENCE_MUTATION"]); assert.ok(!r.diagnostics?.includes("OPEN_POLICY_OBSERVATION_ONLY")); });
