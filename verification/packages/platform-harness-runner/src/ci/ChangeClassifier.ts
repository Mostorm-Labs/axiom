export type PlatformName = "web" | "windows" | "android" | "apple";

export type RunSetManifest = {
  format: "axiom-pr-run-set-v1";
  formatVersion: 1;
  schema: true;
  protocol: true;
  semantic: true;
  selectedPlatforms: PlatformName[];
  changedPaths: string[];
  selectionReason: "SAFE_ALL_UNKNOWN_PATH" | "BROAD_VERIFICATION_BOUNDARY" | "TARGETED_PLATFORM_ADAPTER" | "EMPTY_CHANGESET_SAFE_ALL";
};

const ALL_PLATFORMS: PlatformName[] = ["web", "windows", "android", "apple"];
const adapterPattern = /^verification\/packages\/platform-harness-(web|android|apple)\//;

export function classifyChanges(paths: string[]): RunSetManifest {
  const changedPaths = [...new Set(paths)].sort();
  const selected = new Set<PlatformName>();
  let broad = false;
  let unknown = false;

  for (const path of changedPaths) {
    const adapter = path.match(adapterPattern)?.[1];
    if (adapter) {
      selected.add(adapter as PlatformName);
      continue;
    }
    if (path.startsWith("verification/native/platform/windows/") || path.includes("windows_native")) {
      selected.add("windows");
      continue;
    }
    if (path.startsWith("verification/native/platform/apple/")) {
      selected.add("apple");
      continue;
    }
    if (path.startsWith("verification/native/platform/android/")) {
      selected.add("android");
      continue;
    }
    if (path.startsWith("verification/schemas/") ||
        path.startsWith("verification/platform/") ||
        path.startsWith("verification/packages/platform-harness-protocol/") ||
        path.startsWith("verification/packages/platform-harness-runner/") ||
        path.startsWith("verification/packages/platform-conformance-cli/") ||
        path.startsWith("verification/native/common/") ||
        path.startsWith("verification/tools/") ||
        path.startsWith("verification/tests/") ||
        path === "verification/package.json" ||
        path === "verification/package-lock.json" ||
        path === "verification/README.md" ||
        path.startsWith("pocs/shared_engine/") ||
        path.startsWith(".github/workflows/") ||
        path.startsWith("docs/")) {
      broad = true;
      continue;
    }
    unknown = true;
  }

  const safeAll = broad || unknown || selected.size === 0;
  return {
    format: "axiom-pr-run-set-v1",
    formatVersion: 1,
    schema: true,
    protocol: true,
    semantic: true,
    selectedPlatforms: safeAll ? [...ALL_PLATFORMS] : ALL_PLATFORMS.filter((platform) => selected.has(platform)),
    changedPaths,
    selectionReason: unknown ? "SAFE_ALL_UNKNOWN_PATH" : broad ? "BROAD_VERIFICATION_BOUNDARY" : selected.size === 0 ? "EMPTY_CHANGESET_SAFE_ALL" : "TARGETED_PLATFORM_ADAPTER",
  };
}
