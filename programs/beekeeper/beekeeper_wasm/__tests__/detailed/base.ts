import { chromium, ChromiumBrowser, expect } from '@playwright/test';
import { test } from '../assets/jest-helper.js'
import { WALLET_OPTIONS_NODE } from '../assets/data.js';

let browser!: ChromiumBrowser;

test.describe('WASM Base tests', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });
  });

  test('Should be able to create instance of beekeeper_api', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, cryptoCallbacks }) => {
      // core_minimal constructor: (storage, crypto, unlockTimeout)
      const store = new Map();
      const storage = {
        save_fn: (name, data) => { store.set(name, new Uint8Array(data)); },
        load_fn: (name) => { const d = store.get(name); if (!d) throw new Error("Wallet not found: " + name); return d; },
        list_dir_fn: () => Array.from(store.keys())
      };
      return typeof new provider.beekeeper_api(storage, cryptoCallbacks, 900);
    });

    expect(retVal).toBe('object');
  });

  test('Should be able to create instance of BeekeeperInstanceHelper', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider, BeekeeperInstanceHelper }, WALLET_OPTIONS_NODE) => {
      return new BeekeeperInstanceHelper(provider, WALLET_OPTIONS_NODE);
    }, WALLET_OPTIONS_NODE);

    expect(retVal).toBeDefined();
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
