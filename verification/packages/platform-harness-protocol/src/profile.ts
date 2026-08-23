export const SAFE_ARTIFACT_PATH_PATTERN = /^(?!\/)(?![A-Za-z]:)(?!.*(?:^|\/)\.\.(?:\/|$))(?!.*\/\/)(?!.*\\)[^\s]+$/;
export function isSafeArtifactPath(value: string): boolean { return SAFE_ARTIFACT_PATH_PATTERN.test(value); }
export function asSafeArtifactPath(value: unknown): string {
  if (typeof value !== "string" || !isSafeArtifactPath(value)) throw new TypeError("unsafe artifact path");
  return value;
}
