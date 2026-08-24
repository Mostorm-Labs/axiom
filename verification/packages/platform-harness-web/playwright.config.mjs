import { defineConfig } from "@playwright/test";

export default defineConfig({
  testDir: "./test",
  testMatch: "browser-host.spec.mjs",
  use: { baseURL: "http://127.0.0.1:41873", headless: true },
  webServer: {
    command: "python3 -m http.server 41873 --bind 127.0.0.1 --directory host",
    url: "http://127.0.0.1:41873",
    reuseExistingServer: false,
  },
});
