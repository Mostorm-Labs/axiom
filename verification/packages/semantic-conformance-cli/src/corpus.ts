import type { CaseIntent, CExpectedOutcome } from "./types.js";

export function indexByCaseId<T>(
  records: readonly T[],
  getId: (record: T) => string,
): Map<string, T> {
  const result = new Map<string, T>();
  for (const record of records) {
    const id = getId(record);
    if (result.has(id)) {
      throw new Error(`duplicate case id: ${id}`);
    }
    result.set(id, record);
  }
  return result;
}

export function requireExpectedForCase(
  caseIntent: CaseIntent,
  expectedById: ReadonlyMap<string, CExpectedOutcome>,
): CExpectedOutcome {
  const expected = expectedById.get(caseIntent.id);
  if (!expected) {
    throw new Error(`missing expected outcome: ${caseIntent.id}`);
  }
  return expected;
}
