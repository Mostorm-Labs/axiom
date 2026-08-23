import { readFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const packageJson = JSON.parse(await readFile(resolve(root, "package.json"), "utf8"));
const tsconfig = JSON.parse(await readFile(resolve(root, "tsconfig.base.json"), "utf8"));
if (packageJson.private !== true || packageJson.type !== "module") {
  throw new Error("verification package must be private ESM workspace");
}
if (!Array.isArray(packageJson.workspaces) || packageJson.workspaces.length !== 1) {
  throw new Error("verification workspace must declare packages/* workspace");
}
if (tsconfig.compilerOptions?.strict !== true || tsconfig.compilerOptions?.noEmit !== true) {
  throw new Error("verification TypeScript baseline must be strict and noEmit");
}
console.log("workspace build/typecheck scaffold: valid");
