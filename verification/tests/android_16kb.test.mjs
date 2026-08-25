import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const repositoryRoot = resolve(fileURLToPath(new URL("../..", import.meta.url)));

test("Android instrumentation build declares ELF and APK 16 KB gates", async () => {
  const cmake = await readFile(
    resolve(repositoryRoot, "verification/native/platform/android/CMakeLists.txt"),
    "utf8",
  );
  const gradle = await readFile(
    resolve(
      repositoryRoot,
      "verification/native/platform/android/instrumentation/app/build.gradle.kts",
    ),
    "utf8",
  );
  const workflow = await readFile(
    resolve(repositoryRoot, ".github/workflows/g0-android-instrumentation-adapter.yml"),
    "utf8",
  );

  assert.match(cmake, /-z,max-page-size=16384/);
  assert.match(cmake, /-z,common-page-size=16384/);
  assert.match(gradle, /useLegacyPackaging\s*=\s*false/);
  assert.match(workflow, /"\$ZIPALIGN"\s+-c\s+-P\s+16\s+-v\s+4/);
  assert.match(workflow, /app-debug\.apk/);
  assert.match(workflow, /app-debug-androidTest\.apk/);
  assert.match(workflow, /out\/g0-12-android-alignment/);
  assert.match(workflow, /sha256sum/);
});
