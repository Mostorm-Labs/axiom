import assert from "node:assert/strict";
import test from "node:test";
import { createG3GateReport } from "../dist/ci/G3GateReport.js";

const sha = (char) => char.repeat(40);
const base = () => ({
  sourceCommit: sha("a"), branch: "main", generatorVersion: "0.1.0",
  wpLineage: Array.from({ length: 10 }, (_, i) => ({ taskId: `GT-G3-${String(i + 1).padStart(2, "0")}`, status: "PASS", resultRevision: sha("b"), integrationRevision: sha("c"), evidenceRefs: [`.aegis/results/GT-G3-${String(i + 1).padStart(2, "0")}/result.json`] })),
  checks: ["structural", "runtime", "render", "locality", "stale-generation"].map((id) => ({ id, status: "PASS", required: true, evidenceRefs: [`.aegis/results/GT-G3-11/${id}.json`], issues: [] })),
  platforms: ["web", "windows", "android", "ios", "ipados"].map((subject) => ({ subject, platformFamily: subject === "web" ? "WEB" : subject === "windows" ? "WINDOWS" : subject === "android" ? "ANDROID" : "APPLE", status: "PASS", reality: "PHYSICAL", evidencePath: `verification/evidence/${subject}.json`, sourceCommit: sha("d") })),
  inputSha256: { lineage: "e".repeat(64) },
});

test("G3 report passes only with complete lineage, checks and platforms", () => {
  const report = createG3GateReport(base());
  assert.equal(report.status, "PASS"); assert.equal(report.authority, "G3_GATE_REPORT"); assert.equal(report.promotion.allowed, true);
});
test("missing platform evidence is explicitly BLOCKED", () => {
  const input = base(); input.platforms = input.platforms.filter(({ subject }) => subject !== "windows");
  const report = createG3GateReport(input);
  assert.equal(report.status, "BLOCKED"); assert.equal(report.promotion.allowed, false); assert.ok(report.issues.includes("MISSING_PLATFORM:windows"));
});
test("a closed WP cannot be relabeled PASS when its result identity is invalid", () => {
  const input = base(); input.wpLineage[0].resultRevision = "not-a-sha";
  const report = createG3GateReport(input);
  assert.equal(report.status, "BLOCKED"); assert.ok(report.issues.includes("INVALID_REVISION:GT-G3-01"));
});
