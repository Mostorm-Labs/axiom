export const CLOSED_CURRENT_POLICIES = new Set([
  "connector-target-delete",
  "geometry-point-like-elements-per-operation-aggregate",
]);

export function assertExpectedPolicyStatus(policyKey, expectedRecord) {
  if (CLOSED_CURRENT_POLICIES.has(policyKey) && expectedRecord.openPolicy === true) {
    throw new Error(`closed policy cannot be OPEN: ${policyKey}`);
  }
}
