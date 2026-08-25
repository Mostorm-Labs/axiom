export type PrLayer = "schema" | "protocol" | "semantic" | "platform";
export type LayerStatus = "PASS" | "PASS_WITH_OBSERVATIONS" | "FAIL" | "INVALID_EVIDENCE" | "BLOCKED_AUTHORITY";

export type PrLayerRecord = {
  format: "axiom-pr-layer-record-v1";
  formatVersion: 1;
  layer: PrLayer;
  subject: string;
  attempt: number;
  status: LayerStatus;
  evidenceSha256: string;
  diagnostics: unknown[];
};

export type PrDecision = {
  format: "axiom-pr-decision-v1";
  formatVersion: 1;
  decision: LayerStatus;
  failedLayer: PrLayer | null;
  attempts: PrLayerRecord[];
};

const LAYERS: PrLayer[] = ["schema", "protocol", "semantic", "platform"];

export function aggregatePrDecision(records: PrLayerRecord[]): PrDecision {
  const attempts = [...records].sort((left, right) =>
    LAYERS.indexOf(left.layer) - LAYERS.indexOf(right.layer) || left.subject.localeCompare(right.subject) || left.attempt - right.attempt);
  const latestRecords = [...new Set(attempts.map((record) => `${record.layer}\0${record.subject}`))]
    .map((key) => {
      const [layer, subject] = key.split("\0");
      return attempts.filter((record) => record.layer === layer && record.subject === subject).at(-1)!;
    });
  for (const latest of latestRecords) {
    const layer = latest.layer;
    if (!/^[0-9a-f]{64}$/.test(latest.evidenceSha256) || latest.status === "INVALID_EVIDENCE") {
      return { format: "axiom-pr-decision-v1", formatVersion: 1, decision: "INVALID_EVIDENCE", failedLayer: layer, attempts };
    }
    if (latest.status === "FAIL") {
      return { format: "axiom-pr-decision-v1", formatVersion: 1, decision: "FAIL", failedLayer: layer, attempts };
    }
    if (latest.status === "BLOCKED_AUTHORITY") {
      return { format: "axiom-pr-decision-v1", formatVersion: 1, decision: "BLOCKED_AUTHORITY", failedLayer: layer, attempts };
    }
  }
  for (const layer of LAYERS) {
    if (!attempts.some((record) => record.layer === layer)) {
      return { format: "axiom-pr-decision-v1", formatVersion: 1, decision: "INVALID_EVIDENCE", failedLayer: layer, attempts };
    }
  }
  const hasObservations = attempts.some((record) => record.status === "PASS_WITH_OBSERVATIONS");
  return {
    format: "axiom-pr-decision-v1",
    formatVersion: 1,
    decision: hasObservations ? "PASS_WITH_OBSERVATIONS" : "PASS",
    failedLayer: null,
    attempts,
  };
}
