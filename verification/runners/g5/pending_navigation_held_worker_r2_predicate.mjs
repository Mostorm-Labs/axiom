// A reserved pending intent keeps the SDK's global retry flag true. The
// qualification stage observes actual canonical default-target content writes;
// quiet intervals are checked separately, never inferred from retry.
export function completeContent(phase) {
  return phase.colorOps.some(value => value.kind === 'content');
}
