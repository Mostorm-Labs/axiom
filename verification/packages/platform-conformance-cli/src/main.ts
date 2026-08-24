#!/usr/bin/env node

import { dirname, resolve } from "node:path";
import { realpathSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { protocol } from "./commands/protocol.js";
import { notImplemented } from "./commands/stubs.js";
import { validate } from "./commands/validate.js";
import { profile, runWeb } from "./commands/web.js";
import { ExitCode } from "./exit_codes.js";

const usage = `axiom-platform-conformance <command>

Commands:
  validate
  protocol --suite protocol-seed-v0.1 --boundary in-process|serialized-loopback [--boundary ...] [--output PATH]
  list | compare | aggregate (reserved)

Exit codes: 0 success, 2 invalid arguments, 10 invalid schema/corpus, 20 invalid evidence, 21 runner mismatch, 30 reserved command.`;

function verificationRoot(): string {
  return process.env.AXIOM_VERIFICATION_ROOT
    ? resolve(process.env.AXIOM_VERIFICATION_ROOT)
    : resolve(dirname(fileURLToPath(import.meta.url)), "../../../");
}

export async function main(argv = process.argv.slice(2)): Promise<number> {
  const [command, ...args] = argv;
  if (!command || command === "--help" || command === "help") {
    console.log(usage);
    return ExitCode.SUCCESS;
  }
  if (args.includes("--bless") || args.includes("--update-golden")) {
    console.error("golden or corpus updates are forbidden from the conformance CLI");
    return ExitCode.INVALID_ARGUMENTS;
  }
  if (command === "validate") {
    return args.length === 0 ? validate(verificationRoot()) : ExitCode.INVALID_ARGUMENTS;
  }
  if (command === "protocol") {
    return protocol(verificationRoot(), args);
  }
  if (command === "profile") return profile(args);
  if (command === "run") return runWeb(verificationRoot(), args);
  if (["list", "compare", "aggregate"].includes(command)) {
    return args.length === 0 ? notImplemented() : ExitCode.INVALID_ARGUMENTS;
  }
  return ExitCode.INVALID_ARGUMENTS;
}

const invokedAsScript = process.argv[1]
  ? realpathSync(process.argv[1]) === realpathSync(fileURLToPath(import.meta.url))
  : false;
if (invokedAsScript) main().then((code) => { process.exitCode = code; });
