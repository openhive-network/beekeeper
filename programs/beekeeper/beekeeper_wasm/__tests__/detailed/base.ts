import { chromium, ChromiumBrowser, expect } from '@playwright/test';
import { test } from '../assets/jest-helper.js'

let browser!: ChromiumBrowser;

// Test WIF key (DO NOT USE IN PRODUCTION - this is a well-known test key)
const TEST_WIF = '5HpjE2Hs7vjU4SN3YyPQCdhzCu92WoEeuE6PWNuiPyTu3ESGnzn';
const TEST_DIGEST = '9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08';

test.describe('WASM Base tests', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });
  });

  test('Should be able to validate WIF', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider }, wif: string) => {
      const result = JSON.parse(provider.validate_wif(wif));
      return result.valid;
    }, TEST_WIF);

    expect(retVal).toBe(true);
  });

  test('Should be able to convert WIF to public key', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider }, wif: string) => {
      const result = JSON.parse(provider.wif_to_public_key(wif, 'STM'));
      return result.public_key;
    }, TEST_WIF);

    expect(retVal).toBeDefined();
    expect(typeof retVal).toBe('string');
    expect(retVal.startsWith('STM')).toBe(true);
  });

  test('Should be able to sign a digest', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider }, wif: string, digest: string) => {
      const result = JSON.parse(provider.sign_digest(wif, digest));
      return result.signature;
    }, TEST_WIF, TEST_DIGEST);

    expect(retVal).toBeDefined();
    expect(typeof retVal).toBe('string');
    expect(retVal.length).toBe(130); // 65 bytes as hex = 130 chars
  });

  test('Should be able to get private key bytes', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider }, wif: string) => {
      const result = JSON.parse(provider.get_private_key_bytes(wif));
      return result.private_key;
    }, TEST_WIF);

    expect(retVal).toBeDefined();
    expect(typeof retVal).toBe('string');
    expect(retVal.length).toBe(64); // 32 bytes as hex = 64 chars
  });

  test('Should reject invalid WIF', async ({ beekeeperWasmTest }) => {
    const retVal = await beekeeperWasmTest(async ({ provider }, invalidWif: string) => {
      const result = JSON.parse(provider.validate_wif(invalidWif));
      return result.valid;
    }, 'invalid_wif');

    expect(retVal).toBe(false);
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
