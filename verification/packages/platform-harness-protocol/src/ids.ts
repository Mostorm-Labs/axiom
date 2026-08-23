export type Brand<T, B extends string> = T & { readonly __brand: B };

export type MessageId = Brand<string, "MessageId">;
export type SessionId = Brand<string, "SessionId">;
export type ActionId = Brand<string, "ActionId">;
export type SourceId = Brand<string, "SourceId">;
export type TaggedU64 = Brand<string, "TaggedU64">;

export const MESSAGE_ID_PATTERN = /^msg:[0-9]{5,}$/;
export const SESSION_ID_PATTERN = /^session:[0-9]{3,}$/;
export const ACTION_ID_PATTERN = /^action:[0-9]{3,}$/;
export const SOURCE_ID_PATTERN = /^source:[A-Za-z0-9._-]+$/;
export const TAGGED_U64_PATTERN = /^u64:[0-9a-f]{16}$/;

function branded<B extends string>(value: unknown, pattern: RegExp, name: string): Brand<string, B> {
  if (typeof value !== "string" || !pattern.test(value)) throw new TypeError(`invalid ${name}`);
  return value as Brand<string, B>;
}

export const asMessageId = (value: unknown): MessageId => branded(value, MESSAGE_ID_PATTERN, "messageId");
export const asSessionId = (value: unknown): SessionId => branded(value, SESSION_ID_PATTERN, "sessionId");
export const asActionId = (value: unknown): ActionId => branded(value, ACTION_ID_PATTERN, "actionId");
export const asSourceId = (value: unknown): SourceId => branded(value, SOURCE_ID_PATTERN, "sourceId");
export const asTaggedU64 = (value: unknown): TaggedU64 => branded(value, TAGGED_U64_PATTERN, "tagged u64");
