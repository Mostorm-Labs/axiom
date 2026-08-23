import { asSafeArtifactPath } from "./profile.js";
import { requireStrictKeys, requireString, type JsonObject } from "./schema_loader.js";
export interface CaptureResultPayload extends JsonObject { captureId: string; artifactPath: string; }
export function parseCaptureResult(value: Record<string, unknown>): CaptureResultPayload {
  requireStrictKeys(value, ["captureId", "artifactPath"]);
  return { captureId: requireString(value.captureId, "payload.captureId"), artifactPath: asSafeArtifactPath(value.artifactPath) };
}
