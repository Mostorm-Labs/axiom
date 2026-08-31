import test from "node:test";
import assert from "node:assert/strict";
import { indexByCaseId, requireExpectedForCase } from "../dist/corpus.js";
import { validateCoordinatorInputs } from "../dist/provenance.js";

function base() {
  const id = "CASE";
  return {
    caseIntent: { format: "axiom-g1-04-c-case-v1", formatVersion: 1, provenance: "AUTHORITY_MANUAL", id, operationFamily: "x", authorityRuleRefs: [], inputRef: "input", expectedRef: "expected", blocking: true },
    expected: { format: "axiom-g1-04-c-expected-v1", formatVersion: 1, provenance: "AUTHORITY_MANUAL", caseId: id, authorityRuleRefs: [], mutationExpected: false, disposition: "PLAN_READY", terminalPhase: "PREPARE" },
    reference: { format: "axiom-g1-04-c-observation-v1", formatVersion: 1, provenance: "IMPLEMENTATION_OBSERVATION", caseId: id, provider: "reference", observedDisposition: "PLAN_READY", observedTerminalPhase: "PREPARE", beforeProjection: {}, afterProjection: {} },
    indexed: { format: "axiom-g1-04-c-observation-v1", formatVersion: 1, provenance: "IMPLEMENTATION_OBSERVATION", caseId: id, provider: "indexed", observedDisposition: "PLAN_READY", observedTerminalPhase: "PREPARE", beforeProjection: {}, afterProjection: {} },
    referenceRef: "r", indexedRef: "i", openAuthorityDecision: "UNRESOLVED"
  };
}

test("valid coordinator inputs have no diagnostics", () => assert.deepEqual(validateCoordinatorInputs(base()), []));
test("wrong expected provenance fails closed", () => { const x = base(); x.expected.provenance = "IMPLEMENTATION_OBSERVATION"; assert.ok(validateCoordinatorInputs(x).includes("EXPECTED_PROVENANCE_INVALID")); });
test("wrong reference provenance fails closed", () => { const x = base(); x.reference.provenance = "AUTHORITY_MANUAL"; assert.ok(validateCoordinatorInputs(x).includes("REFERENCE_PROVENANCE_INVALID")); });
test("wrong indexed provenance fails closed", () => { const x = base(); x.indexed.provenance = "AUTHORITY_MANUAL"; assert.ok(validateCoordinatorInputs(x).includes("INDEXED_PROVENANCE_INVALID")); });
test("mismatched case ids fail closed", () => { const x = base(); x.indexed.caseId = "OTHER"; assert.ok(validateCoordinatorInputs(x).includes("CASE_ID_MISMATCH")); });
test("provider identities are exact", () => { const x = base(); x.reference.provider = "indexed"; assert.ok(validateCoordinatorInputs(x).includes("PROVIDER_SET_INVALID")); });
test("closed expected requires disposition and terminal phase", () => { const x = base(); delete x.expected.disposition; assert.ok(validateCoordinatorInputs(x).includes("EXPECTED_CONTRACT_INVALID")); });
test("duplicate corpus ids throw", () => assert.throws(() => indexByCaseId([{ id: "a" }, { id: "a" }], x => x.id), /duplicate case id/));
test("missing expected throws", () => assert.throws(() => requireExpectedForCase(base().caseIntent, new Map()), /missing expected outcome/));
