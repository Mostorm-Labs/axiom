import { COMPLETION_OUTCOMES, type CompletionOutcome } from "./enums.js";
import { asActionId, asCompletionTokenId, type ActionId, type CompletionTokenId } from "./ids.js";
import { isSafeArtifactPath } from "./profile.js";
import { requireStrictKeys, type JsonObject } from "./schema_loader.js";

export interface ActionCompletionPayload extends JsonObject { tokenId: CompletionTokenId; actionId: ActionId; outcome: CompletionOutcome; artifacts: string[]; }

export function parseActionCompletion(value: Record<string, unknown>): ActionCompletionPayload {
  requireStrictKeys(value, ["tokenId", "actionId", "outcome"], ["artifacts"]);
  if (typeof value.outcome !== "string" || !COMPLETION_OUTCOMES.includes(value.outcome as CompletionOutcome)) throw new TypeError("payload.outcome: invalid outcome");
  const artifacts = value.artifacts ?? [];
  if (!Array.isArray(artifacts) || artifacts.some((path) => typeof path !== "string" || !isSafeArtifactPath(path))) throw new TypeError("payload.artifacts: unsafe artifact path");
  return { tokenId: asCompletionTokenId(value.tokenId), actionId: asActionId(value.actionId), outcome: value.outcome as CompletionOutcome, artifacts: [...artifacts] as string[] };
}
