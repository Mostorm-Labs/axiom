export type JsonPrimitive = string | number | boolean | null;
export type JsonValue = JsonPrimitive | JsonValue[] | { [key: string]: JsonValue };
export type JsonObject = { [key: string]: JsonValue };

export function requireRecord(value: unknown, at: string): Record<string, unknown> {
  if (value === null || typeof value !== "object" || Array.isArray(value)) throw new TypeError(`${at}: expected object`);
  return value as Record<string, unknown>;
}

export function requireStrictKeys(value: Record<string, unknown>, required: readonly string[], optional: readonly string[] = []): void {
  for (const key of required) if (!Object.hasOwn(value, key)) throw new TypeError(`payload: missing ${key}`);
  const known = new Set([...required, ...optional]);
  for (const key of Object.keys(value)) if (!known.has(key)) throw new TypeError(`payload: unknown ${key}`);
}

export function requireString(value: unknown, at: string): string {
  if (typeof value !== "string" || value.length === 0) throw new TypeError(`${at}: expected non-empty string`);
  return value;
}

export function requireBoolean(value: unknown, at: string): boolean {
  if (typeof value !== "boolean") throw new TypeError(`${at}: expected boolean`);
  return value;
}

export function requireStringArray(value: unknown, at: string): string[] {
  if (!Array.isArray(value) || value.some((entry) => typeof entry !== "string")) throw new TypeError(`${at}: expected string[]`);
  return [...value];
}
