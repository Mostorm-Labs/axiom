import { readFileSync } from "node:fs";
import { pathToFileURL } from "node:url";
import { coordinateCase } from "./coordinator.js";
import type { OpenAuthorityDecision } from "./types.js";

const UNSAFE = new Set(["--bless", "--update-golden", "--accept-current-output"]);
const REQUIRED = ["--case", "--expected", "--reference", "--indexed", "--reference-ref", "--indexed-ref", "--open-authority"] as const;
const USAGE = "usage: axiom-semantic-conformance compare-case --case <json> --expected <json> --reference <json> --indexed <json> --reference-ref <ref> --indexed-ref <ref> --open-authority CURRENT_OPEN|CURRENT_CLOSED|UNRESOLVED";

function fail(message: string): never {
  throw new Error(message);
}

function parseArgs(argv: string[]): Record<string, string> {
  if (argv[0] !== "compare-case") fail("unknown command");
  const parsed: Record<string, string> = {};
  for (let i = 1; i < argv.length; i += 2) {
    const key = argv[i];
    if (UNSAFE.has(key)) fail("unsafe option is forbidden");
    if (!REQUIRED.includes(key as (typeof REQUIRED)[number])) fail("unknown option");
    const value = argv[i + 1];
    if (value === undefined || value.startsWith("--")) fail("missing option value");
    if (parsed[key] !== undefined) fail("duplicate option");
    parsed[key] = value;
  }
  for (const key of REQUIRED) {
    if (!parsed[key]) fail(`missing ${key}`);
  }
  const decision = parsed["--open-authority"];
  if (!(["CURRENT_OPEN", "CURRENT_CLOSED", "UNRESOLVED"] as string[]).includes(decision)) {
    fail("invalid open authority decision");
  }
  return parsed;
}

function readJson(path: string): unknown {
  return JSON.parse(readFileSync(path, "utf8"));
}

export function main(argv = process.argv.slice(2)): void {
  try {
    const args = parseArgs(argv);
    const result = coordinateCase({
      caseIntent: readJson(args["--case"]) as any,
      expected: readJson(args["--expected"]) as any,
      reference: readJson(args["--reference"]) as any,
      indexed: readJson(args["--indexed"]) as any,
      referenceRef: args["--reference-ref"],
      indexedRef: args["--indexed-ref"],
      openAuthorityDecision: args["--open-authority"] as OpenAuthorityDecision,
    });
    process.stdout.write(`${JSON.stringify(result)}\n`);
  } catch {
    process.stderr.write(`${USAGE}\n`);
    process.exitCode = 2;
  }
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  main();
}
