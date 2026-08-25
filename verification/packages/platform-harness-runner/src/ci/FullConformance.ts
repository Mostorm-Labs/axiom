export type FullCadence = "NIGHTLY" | "RELEASE";
export type FullDecisionStatus = "PASS" | "PASS_WITH_OBSERVATIONS" | "FAIL" | "INVALID_EVIDENCE" | "BLOCKED_AUTHORITY";
export type FullReality = "HOSTED" | "PHYSICAL" | "EMULATOR" | "SIMULATOR" | "NOT_APPLICABLE";
export type FullPlatformKey = "web" | "windows" | "android" | "ios" | "ipados";
export type FullPlatformFamily = "WEB" | "WINDOWS" | "ANDROID" | "APPLE";

export type FullRunSetManifest = {
  format: "axiom-full-run-set-v1";
  formatVersion: 1;
  authority: "G0_WIRING_ONLY";
  cadence: FullCadence;
  sourceCommit: string;
  schemaSha256: string;
  corpusSha256: string;
  runnerVersion: string;
  runtimeVersion: string;
  repeat: number;
  seed: number;
  requiredProfiles: Array<{
    profileKey: FullPlatformKey;
    platformFamily: FullPlatformFamily;
    profileId: string;
    requiredReality: "HOSTED" | "PHYSICAL";
  }>;
};

export type PlatformEvidenceRecord = {
  format: "axiom-platform-evidence-record-v1";
  formatVersion: 1;
  subject: string;
  category: "PREREQUISITE" | "PROFILE";
  platformFamily: FullPlatformFamily | null;
  profileId: string | null;
  sourceCommit: string;
  corpusSha256: string;
  runnerVersion: string;
  runtimeVersion: string;
  reality: FullReality;
  status: FullDecisionStatus;
  evidenceSha256: string;
  pgStatuses: Array<{ group: string; status: FullDecisionStatus }>;
  diagnostics: unknown[];
  environment: Record<string, unknown>;
};

export type PlatformEvidenceIndex = {
  format: "axiom-platform-evidence-index-v1";
  formatVersion: 1;
  authority: "G0_WIRING_ONLY";
  sourceCommit: string;
  corpusSha256: string;
  runnerVersion: string;
  runtimeVersion: string;
  records: PlatformEvidenceRecord[];
  missingSubjects: string[];
  validationIssues: string[];
};

export type PlatformReleaseDecision = {
  format: "axiom-platform-release-decision-v1";
  formatVersion: 1;
  authority: "G0_WIRING_ONLY";
  cadence: FullCadence;
  decision: FullDecisionStatus;
  failedSubject: string | null;
  blockingReasons: string[];
  identity: Pick<FullRunSetManifest, "sourceCommit" | "schemaSha256" | "corpusSha256" | "runnerVersion" | "runtimeVersion" | "repeat" | "seed">;
  pgStatuses: Array<{ group: string; status: FullDecisionStatus }>;
  evidenceSubjects: string[];
};

export type ReproducibilityComparison = {
  format: "axiom-reproducibility-comparison-v1";
  formatVersion: 1;
  authority: "G0_WIRING_ONLY";
  comparable: boolean;
  status: "PASS" | "INVALID_EVIDENCE";
  blockingConclusionMatch: boolean;
  correctnessDifferences: string[];
  environmentDifferences: string[];
};

const profiles: FullRunSetManifest["requiredProfiles"] = [
  { profileKey: "web", platformFamily: "WEB", profileId: "web-reference-v0-1", requiredReality: "HOSTED" },
  { profileKey: "windows", platformFamily: "WINDOWS", profileId: "windows-native-reference-v0-1", requiredReality: "HOSTED" },
  { profileKey: "android", platformFamily: "ANDROID", profileId: "android-instrumentation-reference-v0-1", requiredReality: "HOSTED" },
  { profileKey: "ios", platformFamily: "APPLE", profileId: "ios-rn-objcxx-reference-v0-1", requiredReality: "HOSTED" },
  { profileKey: "ipados", platformFamily: "APPLE", profileId: "ipados-rn-objcxx-reference-v0-1", requiredReality: "HOSTED" },
];
const prerequisites = ["schema", "protocol", "semantic"] as const;
const groups = ["PG-01", "PG-02", "PG-03", "PG-04", "PG-05", "PG-06"] as const;
const sha256 = /^[0-9a-f]{64}$/u;
const commit = /^[0-9a-f]{40}$/u;

export function createFullRunSet(options: Omit<FullRunSetManifest, "format" | "formatVersion" | "authority" | "requiredProfiles">): FullRunSetManifest {
  if (!commit.test(options.sourceCommit)) throw new Error("sourceCommit must be a 40-character lowercase SHA");
  for (const field of ["schemaSha256", "corpusSha256"] as const) if (!sha256.test(options[field])) throw new Error(`${field} must be a lowercase SHA-256`);
  if (!Number.isInteger(options.repeat) || options.repeat < 1) throw new Error("repeat must be a positive integer");
  if (!Number.isInteger(options.seed) || options.seed < 0) throw new Error("seed must be a non-negative integer");
  const requiredReality = options.cadence === "RELEASE" ? "PHYSICAL" : "HOSTED";
  return {
    format: "axiom-full-run-set-v1", formatVersion: 1, authority: "G0_WIRING_ONLY",
    cadence: options.cadence, sourceCommit: options.sourceCommit, schemaSha256: options.schemaSha256,
    corpusSha256: options.corpusSha256, runnerVersion: options.runnerVersion, runtimeVersion: options.runtimeVersion,
    repeat: options.repeat, seed: options.seed,
    requiredProfiles: profiles.map((profile) => ({ ...profile, requiredReality })),
  };
}

export function createPlatformEvidenceIndex(runSet: FullRunSetManifest, records: PlatformEvidenceRecord[]): PlatformEvidenceIndex {
  const sorted = [...records].sort((left, right) => left.subject.localeCompare(right.subject));
  const required = [...prerequisites, ...runSet.requiredProfiles.map(({ profileKey }) => profileKey)];
  const seen = new Set<string>();
  const validationIssues: string[] = [];
  for (const record of sorted) {
    if (seen.has(record.subject)) validationIssues.push(`DUPLICATE_SUBJECT:${record.subject}`);
    seen.add(record.subject);
    if (!sha256.test(record.evidenceSha256)) validationIssues.push(`INVALID_EVIDENCE_HASH:${record.subject}`);
    if (record.sourceCommit !== runSet.sourceCommit) validationIssues.push(`SOURCE_COMMIT_MISMATCH:${record.subject}`);
    if (record.corpusSha256 !== runSet.corpusSha256) validationIssues.push(`CORPUS_HASH_MISMATCH:${record.subject}`);
    if (record.runnerVersion !== runSet.runnerVersion) validationIssues.push(`RUNNER_VERSION_MISMATCH:${record.subject}`);
    if (record.runtimeVersion !== runSet.runtimeVersion) validationIssues.push(`RUNTIME_VERSION_MISMATCH:${record.subject}`);
    if (!prerequisites.includes(record.subject as typeof prerequisites[number])) {
      const expected = runSet.requiredProfiles.find(({ profileKey }) => profileKey === record.subject);
      if (!expected) validationIssues.push(`UNKNOWN_SUBJECT:${record.subject}`);
      else if (record.profileId !== expected.profileId) validationIssues.push(`PROFILE_ID_MISMATCH:${record.subject}`);
    }
  }
  const missingSubjects = required.filter((subject) => !seen.has(subject));
  return {
    format: "axiom-platform-evidence-index-v1", formatVersion: 1, authority: "G0_WIRING_ONLY",
    sourceCommit: runSet.sourceCommit, corpusSha256: runSet.corpusSha256,
    runnerVersion: runSet.runnerVersion, runtimeVersion: runSet.runtimeVersion,
    records: sorted, missingSubjects, validationIssues,
  };
}

const priority: FullDecisionStatus[] = ["INVALID_EVIDENCE", "BLOCKED_AUTHORITY", "FAIL", "PASS_WITH_OBSERVATIONS", "PASS"];
function worse(left: FullDecisionStatus, right: FullDecisionStatus): FullDecisionStatus { return priority.indexOf(left) < priority.indexOf(right) ? left : right; }

export function aggregatePlatformRelease(runSet: FullRunSetManifest, index: PlatformEvidenceIndex): PlatformReleaseDecision {
  const reasons = [...index.validationIssues];
  for (const missing of index.missingSubjects) reasons.push(`MISSING_REQUIRED_SUBJECT:${missing}`);
  const records = new Map(index.records.map((record) => [record.subject, record]));
  for (const profile of runSet.requiredProfiles) {
    const record = records.get(profile.profileKey);
    if (record && record.reality !== profile.requiredReality) reasons.push(`REQUIRED_REALITY_MISSING:${profile.profileKey}:${profile.requiredReality}`);
  }
  let decision: FullDecisionStatus = reasons.length ? "INVALID_EVIDENCE" : "PASS";
  let failedSubject: string | null = reasons[0]?.split(":").slice(-1)[0] ?? null;
  const pg = groups.map((group) => ({ group, status: "PASS" as FullDecisionStatus }));
  for (const record of index.records) {
    decision = worse(decision, record.status);
    if (record.status !== "PASS" && !failedSubject) failedSubject = record.subject;
    for (const entry of record.pgStatuses) {
      const target = pg.find((value) => value.group === entry.group);
      if (target) target.status = worse(target.status, entry.status);
    }
  }
  const authorityReasons = reasons.filter((reason) => reason.startsWith("REQUIRED_REALITY_MISSING:"));
  if (authorityReasons.length && decision === "INVALID_EVIDENCE") decision = "BLOCKED_AUTHORITY";
  if (decision === "BLOCKED_AUTHORITY" && !failedSubject) failedSubject = authorityReasons[0]?.split(":")[1] ?? null;
  return {
    format: "axiom-platform-release-decision-v1", formatVersion: 1, authority: "G0_WIRING_ONLY", cadence: runSet.cadence,
    decision, failedSubject, blockingReasons: reasons,
    identity: { sourceCommit: runSet.sourceCommit, schemaSha256: runSet.schemaSha256, corpusSha256: runSet.corpusSha256, runnerVersion: runSet.runnerVersion, runtimeVersion: runSet.runtimeVersion, repeat: runSet.repeat, seed: runSet.seed },
    pgStatuses: pg, evidenceSubjects: index.records.map(({ subject }) => subject),
  };
}

export function comparePlatformReleaseDecisions(left: PlatformReleaseDecision, right: PlatformReleaseDecision): ReproducibilityComparison {
  const differences: string[] = [];
  for (const field of ["sourceCommit", "schemaSha256", "corpusSha256", "runnerVersion", "runtimeVersion", "repeat", "seed"] as const) {
    if (left.identity[field] !== right.identity[field]) differences.push(`IDENTITY_MISMATCH:${field}`);
  }
  const comparable = differences.length === 0;
  const blockingConclusionMatch = comparable && left.decision === right.decision && JSON.stringify(left.blockingReasons) === JSON.stringify(right.blockingReasons);
  return {
    format: "axiom-reproducibility-comparison-v1", formatVersion: 1, authority: "G0_WIRING_ONLY", comparable,
    status: comparable && blockingConclusionMatch ? "PASS" : "INVALID_EVIDENCE", blockingConclusionMatch,
    correctnessDifferences: differences, environmentDifferences: [],
  };
}
