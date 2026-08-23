import { COMPLETION_OUTCOMES, type CompletionOutcome } from "./enums.js";
import { asActionId, type ActionId } from "./ids.js";
import { isSafeArtifactPath } from "./profile.js";
import { requireStrictKeys, type JsonObject } from "./schema_loader.js";

export interface ActionCompletionPayload extends JsonObject { actionId: ActionId; outcome: CompletionOutcome; artifacts: string[]; }

export function parseActionCompletion(value: Record<string, unknown>): ActionCompletionPayload {
  requireStrictKeys(value, ["actionId", "outcome", "artifacts"]);
  if (typeof value.outcome !== "string" || !COMPLETION_OUTCOMES.includes(value.outcome as CompletionOutcome)) throw new TypeError("payload.outcome: invalid outcome");
  if (!Array.isArray(value.artifacts) || value.artifacts.some((path) => typeof path !== "string" || !isSafeArtifactPath(path))) throw new TypeError("payload.artifacts: unsafe artifact path");
  return { actionId: asActionId(value.actionId), outcome: value.outcome as CompletionOutcome, artifacts: [...value.artifacts] as string[] };
}
