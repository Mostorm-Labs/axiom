export type G3Status = "PASS" | "FAIL" | "BLOCKED";
export type G3Check = { id: string; status: G3Status; required: boolean; evidenceRefs: string[]; issues: string[] };
export type G3WpLineage = {
  taskId: string;
  status: "PASS" | "PASS_WITH_FINDINGS";
  resultRevision: string;
  integrationRevision: string;
  evidenceRefs: string[];
};
export type G3Platform = {
  subject: string;
  platformFamily: "WEB" | "WINDOWS" | "ANDROID" | "APPLE";
  status: G3Status;
  reality: "PHYSICAL" | "HOSTED" | "NOT_APPLICABLE";
  evidencePath: string;
  sourceCommit: string;
};
export type G3GateReport = {
  format: "axiom-g3-gate-report-v1";
  formatVersion: 1;
  authority: "G3_GATE_REPORT";
  gate: "G3";
  status: G3Status;
  sourceCommit: string;
  branch: string;
  generatorVersion: string;
  wpLineage: G3WpLineage[];
  checks: G3Check[];
  platforms: G3Platform[];
  issues: string[];
  inputSha256: Record<string, string>;
  promotion: { allowed: boolean; reason: string };
};

const commit = /^[0-9a-f]{40}$/u;
const sha = /^[0-9a-f]{64}$/u;
const requiredTasks = Array.from({ length: 10 }, (_, index) => `GT-G3-${String(index + 1).padStart(2, "0")}`);
const requiredPlatforms = ["web", "windows", "android", "ios", "ipados"];

function safePath(value: string): boolean { return value.length > 0 && !value.startsWith("/") && !value.includes("..") && !value.includes("\\"); }

export type G3GateReportInput = Omit<G3GateReport, "format" | "formatVersion" | "authority" | "gate" | "status" | "issues" | "promotion">;

export function createG3GateReport(input: G3GateReportInput): G3GateReport {
  if (!commit.test(input.sourceCommit)) throw new Error("sourceCommit must be a 40-character lowercase SHA");
  if (!input.branch) throw new Error("branch is required");
  if (!input.generatorVersion) throw new Error("generatorVersion is required");
  const issues: string[] = [];
  const ids = new Set(input.wpLineage.map((entry) => entry.taskId));
  for (const taskId of requiredTasks) if (!ids.has(taskId)) issues.push(`MISSING_WP:${taskId}`);
  if (ids.size !== input.wpLineage.length) issues.push("DUPLICATE_WP");
  for (const entry of input.wpLineage) {
    if (!/^GT-G3-(?:0[1-9]|10)$/u.test(entry.taskId)) issues.push(`UNKNOWN_WP:${entry.taskId}`);
    if (!commit.test(entry.resultRevision) || !commit.test(entry.integrationRevision)) issues.push(`INVALID_REVISION:${entry.taskId}`);
    if (entry.status !== "PASS" && entry.status !== "PASS_WITH_FINDINGS") issues.push(`WP_NOT_CLOSED:${entry.taskId}`);
    if (!entry.evidenceRefs.length || entry.evidenceRefs.some((ref) => !safePath(ref))) issues.push(`INVALID_WP_EVIDENCE:${entry.taskId}`);
  }
  const platformIds = new Set(input.platforms.map((entry) => entry.subject));
  for (const subject of requiredPlatforms) if (!platformIds.has(subject)) issues.push(`MISSING_PLATFORM:${subject}`);
  if (platformIds.size !== input.platforms.length) issues.push("DUPLICATE_PLATFORM");
  for (const entry of input.platforms) {
    if (!safePath(entry.evidencePath) || !commit.test(entry.sourceCommit)) issues.push(`INVALID_PLATFORM:${entry.subject}`);
    if (!requiredPlatforms.includes(entry.subject)) issues.push(`UNKNOWN_PLATFORM:${entry.subject}`);
    if (entry.status !== "PASS") issues.push(`PLATFORM_${entry.status}:${entry.subject}`);
  }
  const checks = input.checks.map((check) => ({ ...check, evidenceRefs: [...check.evidenceRefs], issues: [...check.issues] }));
  for (const check of checks) {
    if (!check.id || !check.evidenceRefs.length || check.evidenceRefs.some((ref) => !safePath(ref))) issues.push(`INVALID_CHECK:${check.id}`);
    if (check.required && check.status !== "PASS") issues.push(`REQUIRED_CHECK_${check.status}:${check.id}`);
  }
  for (const [key, value] of Object.entries(input.inputSha256)) if (!sha.test(value)) issues.push(`INVALID_INPUT_HASH:${key}`);
  const status: G3Status = issues.some((issue) => issue.startsWith("REQUIRED_CHECK_FAIL") || issue.startsWith("WP_NOT_CLOSED")) ? "FAIL" : issues.length ? "BLOCKED" : "PASS";
  const ordered = [...input.wpLineage].sort((a, b) => a.taskId.localeCompare(b.taskId));
  const platforms = [...input.platforms].sort((a, b) => a.subject.localeCompare(b.subject));
  return {
    format: "axiom-g3-gate-report-v1", formatVersion: 1, authority: "G3_GATE_REPORT", gate: "G3", status,
    sourceCommit: input.sourceCommit, branch: input.branch, generatorVersion: input.generatorVersion,
    wpLineage: ordered, checks, platforms, issues,
    inputSha256: Object.fromEntries(Object.entries(input.inputSha256).sort(([a], [b]) => a.localeCompare(b))),
    promotion: { allowed: status === "PASS", reason: status === "PASS" ? "All G3 obligations passed" : "G3 Gate is not eligible for promotion" },
  };
}
