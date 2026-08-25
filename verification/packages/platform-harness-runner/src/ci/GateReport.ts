export type GateTaskStatus = "Not Started" | "Analyzing" | "Ready" | "Implementing" | "Validating" | "Pass" | "Fail" | "Blocked";
export type GateStatus = "PASS" | "FAIL" | "BLOCKED";
export type CheckLevel = "E1" | "E2" | "E3" | "E4";

export type GateTaskLineage = {
  taskId: string;
  status: GateTaskStatus;
  evidencePath: string;
  evidenceSha256: string;
};

export type GateArtifact = { path: string; bytes: number; sha256: string };
export type GatePlatformRecord = {
  subject: string;
  platformFamily: "WEB" | "WINDOWS" | "ANDROID" | "APPLE" | null;
  profileId: string | null;
  reality: "HOSTED" | "PHYSICAL" | "EMULATOR" | "SIMULATOR" | "NOT_APPLICABLE";
  evidencePath: string;
  environment: Record<string, unknown>;
};

export type G0GateReportInput = {
  sourceCommit: string;
  branch: string;
  schemaVersion: string;
  generatorVersion: string;
  corpus: { schemaSha256: string; corpusSha256: string; runnerVersion: string; runtimeVersion: string };
  taskLineage: GateTaskLineage[];
  platforms: GatePlatformRecord[];
  hosted: { nightlyDecision: "PASS" | "FAIL" | "BLOCKED_AUTHORITY"; releaseDecision: "PASS" | "FAIL" | "BLOCKED_AUTHORITY"; reproducibility: "PASS" | "INVALID_EVIDENCE" };
  artifacts: GateArtifact[];
};

export type GateCheck = {
  level: CheckLevel;
  status: GateStatus;
  applicable: boolean;
  reason: string;
  firstDivergence: null | { taskId: string; field: string; expected: string; actual: string };
  issues: string[];
};

export type G0GateReport = {
  format: "axiom-gate-report-v1";
  formatVersion: 1;
  authority: "G0_GATE_REPORT";
  gate: "G0";
  status: GateStatus;
  sourceCommit: string;
  branch: string;
  schemaVersion: string;
  generatorVersion: string;
  corpus: G0GateReportInput["corpus"];
  platforms: GatePlatformRecord[];
  checks: GateCheck[];
  lineage: GateTaskLineage[];
  artifacts: GateArtifact[];
  issues: string[];
  promotion: { allowed: boolean; reason: string };
};

const sha256 = /^[0-9a-f]{64}$/u;
const commit = /^[0-9a-f]{40}$/u;
const requiredTasks = Array.from({ length: 16 }, (_, index) => `GT-G0-${String(index).padStart(2, "0")}`);
const taskNumber = (taskId: string): number => Number(taskId.slice(-2));

function validateInput(input: G0GateReportInput): void {
  if (!commit.test(input.sourceCommit)) throw new Error("sourceCommit must be a 40-character lowercase SHA");
  for (const hash of [input.corpus.schemaSha256, input.corpus.corpusSha256]) if (!sha256.test(hash)) throw new Error("corpus hashes must be lowercase SHA-256");
  const byId = new Map(input.taskLineage.map((task) => [task.taskId, task]));
  for (const taskId of requiredTasks) if (!byId.has(taskId)) throw new Error(`missing required task ${taskId}`);
  if (byId.size !== input.taskLineage.length) throw new Error("duplicate task lineage");
  for (const task of input.taskLineage) {
    if (!/^GT-G0-(?:0[0-9]|1[0-5])$/u.test(task.taskId)) throw new Error(`unknown task ${task.taskId}`);
    if (!sha256.test(task.evidenceSha256)) throw new Error(`invalid evidence hash ${task.taskId}`);
    if (!task.evidencePath || task.evidencePath.startsWith("/") || task.evidencePath.includes("..")) throw new Error(`unsafe evidence path ${task.taskId}`);
  }
  const platformSubjects = new Set<string>();
  for (const platform of input.platforms) {
    if (!platform.subject || platformSubjects.has(platform.subject)) throw new Error(`duplicate platform ${platform.subject}`);
    platformSubjects.add(platform.subject);
    if (!platform.evidencePath || platform.evidencePath.startsWith("/") || platform.evidencePath.includes("..")) throw new Error(`unsafe platform evidence path ${platform.subject}`);
    if (platform.profileId !== null && !platform.profileId) throw new Error(`invalid platform profile ${platform.subject}`);
  }
  for (const artifact of input.artifacts) {
    if (!artifact.path || artifact.path.startsWith("/") || artifact.path.includes("..")) throw new Error(`unsafe artifact path ${artifact.path}`);
    if (!Number.isInteger(artifact.bytes) || artifact.bytes < 0 || !sha256.test(artifact.sha256)) throw new Error(`invalid artifact ${artifact.path}`);
  }
}

function check(level: CheckLevel, tasks: GateTaskLineage[], reason: string): GateCheck {
  const failed = tasks.find((task) => task.status === "Fail" || task.status === "Blocked");
  const status: GateStatus = failed ? (failed.status === "Blocked" ? "BLOCKED" : "FAIL") : "PASS";
  return {
    level, status, applicable: true, reason,
    firstDivergence: failed ? { taskId: failed.taskId, field: "status", expected: "Pass", actual: failed.status } : null,
    issues: failed ? [`${failed.taskId}:${failed.status}`] : [],
  };
}

export function createG0GateReport(input: G0GateReportInput): G0GateReport {
  validateInput(input);
  const ordered = [...input.taskLineage].sort((left, right) => left.taskId.localeCompare(right.taskId));
  const e1 = check("E1", ordered.filter((task) => taskNumber(task.taskId) <= 9), "Contract, schema, runner and corpus foundations");
  const e2 = check("E2", ordered.filter((task) => taskNumber(task.taskId) >= 10 && taskNumber(task.taskId) <= 13), "Reference platform adapters");
  const hostedIssues: string[] = [];
  if (input.hosted.nightlyDecision !== "PASS") hostedIssues.push(`NIGHTLY:${input.hosted.nightlyDecision}`);
  if (input.hosted.reproducibility !== "PASS") hostedIssues.push(`REPRODUCIBILITY:${input.hosted.reproducibility}`);
  const e3: GateCheck = {
    level: "E3", status: hostedIssues.length ? "FAIL" : "PASS", applicable: true,
    reason: "PR, Nightly and reproducibility integration evidence", firstDivergence: null, issues: hostedIssues,
  };
  const e4: GateCheck = {
    level: "E4", status: input.hosted.releaseDecision === "BLOCKED_AUTHORITY" ? "BLOCKED" : input.hosted.releaseDecision === "PASS" ? "PASS" : "FAIL",
    applicable: true, reason: "Physical release authority is required for platform promotion", firstDivergence: null,
    issues: input.hosted.releaseDecision === "BLOCKED_AUTHORITY" ? ["PHYSICAL_RELEASE_AUTHORITY_BLOCKED"] : [],
  };
  const checks = [e1, e2, e3, e4];
  const issues = checks.flatMap(({ issues: values }) => values);
  const status: GateStatus = checks.some(({ status: value }) => value === "FAIL") ? "FAIL" : checks.some(({ status: value }) => value === "BLOCKED") ? "BLOCKED" : "PASS";
  return {
    format: "axiom-gate-report-v1", formatVersion: 1, authority: "G0_GATE_REPORT", gate: "G0", status,
    sourceCommit: input.sourceCommit, branch: input.branch, schemaVersion: input.schemaVersion, generatorVersion: input.generatorVersion,
    corpus: input.corpus, checks, lineage: ordered, artifacts: [...input.artifacts].sort((left, right) => left.path.localeCompare(right.path)), issues,
    platforms: [...input.platforms].sort((left, right) => left.subject.localeCompare(right.subject)),
    promotion: { allowed: status === "PASS", reason: status === "PASS" ? "All applicable checks passed" : "Gate is not eligible for promotion" },
  };
}
