export type C7ConditionId =
  | "authority-provenance"
  | "mandatory-corpus"
  | "manual-golden-correctness"
  | "no-mutation"
  | "provider-differential"
  | "fixture-reproducibility"
  | "open-reconciliation";

export interface C7ConditionResult {
  id: C7ConditionId;
  status: "PASS" | "FAIL";
  evidenceRefs: string[];
}

export interface C7GateSummary {
  status: "PASS" | "FAIL";
  conditions: C7ConditionResult[];
  failedConditions: C7ConditionId[];
}

const conditionIds: readonly C7ConditionId[] = [
  "authority-provenance",
  "mandatory-corpus",
  "manual-golden-correctness",
  "no-mutation",
  "provider-differential",
  "fixture-reproducibility",
  "open-reconciliation",
];

function asRecord(value: unknown, label: string): Record<string, unknown> {
  if (typeof value !== "object" || value === null || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value as Record<string, unknown>;
}

export function evaluateC7Gate(input: unknown): C7GateSummary {
  const root = asRecord(input, "C7 gate input");
  if (!Array.isArray(root.conditions) || root.conditions.length !== conditionIds.length) throw new Error("C7 gate must contain exactly seven conditions");
  const provided = new Map<C7ConditionId, C7ConditionResult>();
  for (const raw of root.conditions) {
    const condition = asRecord(raw, "C7 gate condition");
    if (typeof condition.id !== "string" || !conditionIds.includes(condition.id as C7ConditionId)) throw new Error("C7 gate contains an unknown condition");
    const id = condition.id as C7ConditionId;
    if (provided.has(id)) throw new Error(`duplicate C7 gate condition: ${id}`);
    if (condition.status !== "PASS" && condition.status !== "FAIL") throw new Error(`invalid C7 gate condition status: ${id}`);
    if (!Array.isArray(condition.evidenceRefs) || condition.evidenceRefs.length === 0 || condition.evidenceRefs.some((ref) => typeof ref !== "string" || ref.length === 0) || new Set(condition.evidenceRefs).size !== condition.evidenceRefs.length) {
      throw new Error(`invalid C7 gate condition evidence refs: ${id}`);
    }
    provided.set(id, { id, status: condition.status, evidenceRefs: [...condition.evidenceRefs] });
  }
  if (provided.size !== conditionIds.length || conditionIds.some((id) => !provided.has(id))) throw new Error("C7 gate conditions are incomplete");
  const conditions = conditionIds.map((id) => provided.get(id) as C7ConditionResult);
  const failedConditions = conditions.filter((condition) => condition.status === "FAIL").map((condition) => condition.id);
  return { status: failedConditions.length === 0 ? "PASS" : "FAIL", conditions, failedConditions };
}
