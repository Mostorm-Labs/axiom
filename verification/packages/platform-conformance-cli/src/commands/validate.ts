import { spawnSync } from "node:child_process";
import { ExitCode } from "../exit_codes.js";

export function validate(workspaceRoot: string): number {
  const checks = [
    ["tools/validate_schemas.mjs"],
    ["--test", "tests/schema_meta.test.mjs", "tests/protocol_vectors.test.mjs"],
  ];
  for (const args of checks) {
    const result = spawnSync(process.execPath, args, {
      cwd: workspaceRoot,
      encoding: "utf8",
    });
    if (result.stdout) process.stdout.write(result.stdout);
    if (result.stderr) process.stderr.write(result.stderr);
    if (result.status !== 0) return ExitCode.INVALID_SCHEMA_OR_CORPUS;
  }
  return ExitCode.SUCCESS;
}
