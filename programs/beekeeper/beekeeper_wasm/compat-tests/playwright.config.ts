import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: '.',
  testMatch: 'compat.spec.ts',
  workers: 1,
  webServer: {
    // Serve this directory; test.html maps /new/ and /old/ via importmap
    // We use a simple script to set up symlinks and serve
    command: `node serve.mjs`,
    url: 'http://localhost:8090',
    reuseExistingServer: false,
    timeout: 30_000,
  },
});
