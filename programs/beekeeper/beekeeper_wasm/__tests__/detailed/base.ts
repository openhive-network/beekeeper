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
      // core_minimal constructor: (save_fn, load_fn, crypto, unlockTimeout)
      const store = new Map();
      const save_fn = (name, data) => { store.set(name, new Uint8Array(data)); };
      const load_fn = (name) => { const d = store.get(name); if (!d) throw new Error("Wallet not found: " + name); return d; };
      return typeof new provider.beekeeper_api(save_fn, load_fn, cryptoCallbacks, 900);
    });

    expect(retVal).toBe('object');
  });

  test('Should be able to create instance of ExtractError - import script', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ ExtractError }) => {
      return new ExtractError({ a: 10 }).parsed;
    });

    expect(retVal).toStrictEqual({ a: 10 });
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
