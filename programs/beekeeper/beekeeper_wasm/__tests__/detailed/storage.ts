/**
 * This file tests web browser storage functionality using IndexedDB-backed
 * save/load callbacks passed to the beekeeper WASM module.
 *
 * createIdbStorage() is defined in globals.js and available via globalThis.
 */

import { ChromiumBrowser, Page, chromium } from 'playwright';
import { expect } from '@playwright/test';
import { IBeekeeperTest, test } from '../assets/jest-helper';

let browser: ChromiumBrowser;
let page1: Page, page2: Page, page3: Page;

const DB_NAME = 'beekeeper_wallets';

const saveKeys = async(beekeeperWasmTestWebOnlyWithPage: IBeekeeperTest['beekeeperWasmTestWebOnlyWithPage'], page: Page, options: { dbName: string, close: boolean }) =>
  await beekeeperWasmTestWebOnlyWithPage(page, async({ provider, BeekeeperInstanceHelper }, { dbName, close }) => {
    const storage = createIdbStorage(dbName);

    await storage.syncFromIdb();

    const api = new BeekeeperInstanceHelper(provider, [], { save_fn: storage.save_fn, load_fn: storage.load_fn });

    await api.create_with_password(api.implicitSessionToken, "w0", "badf00d");
    await api.importKey(api.implicitSessionToken, "w0", "5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT");

    await storage.syncToIdb();

    if(close) {
      api.close(api.implicitSessionToken, "w0");
      api.closeSession(api.implicitSessionToken);
      api.deleteInstance();
    }

    await storage.syncToIdb();

    return await storage.listKeys();
  }, options);

const retrieveKeys = async(beekeeperWasmTestWebOnlyWithPage: IBeekeeperTest['beekeeperWasmTestWebOnlyWithPage'], page: Page, options: { dbName: string, close: boolean }) =>
  await beekeeperWasmTestWebOnlyWithPage(page, async({ provider, BeekeeperInstanceHelper }, { dbName, close }) => {
    const storage = createIdbStorage(dbName);

    await storage.syncFromIdb();

    const api = new BeekeeperInstanceHelper(provider, [], { save_fn: storage.save_fn, load_fn: storage.load_fn });

    api.open(api.implicitSessionToken, "w0");
    await api.unlock(api.implicitSessionToken, "w0", "badf00d");

    const keys = api.getPublicKeys(api.implicitSessionToken);

    if(close) {
      api.close(api.implicitSessionToken, "w0");
      api.closeSession(api.implicitSessionToken);
      api.deleteInstance();
    }

    return keys;
  }, options);

test.describe('WASM storage tests', () => {
  test.beforeAll(async () => {
    browser = await chromium.launch({
      headless: true
    });

    const context1 = await browser.newContext();
    const context2 = await browser.newContext();

    page1 = await context1.newPage();
    page2 = await context1.newPage();
    page3 = await context2.newPage();
  });

  test('Should be able to persist data to IndexedDB and read it back', async ({ beekeeperWasmTestWebOnlyWithPage }) => {
    const result = await beekeeperWasmTestWebOnlyWithPage(page1, async ({ provider, BeekeeperInstanceHelper }, dbName) => {
      const storage = createIdbStorage(dbName);

      const api = new BeekeeperInstanceHelper(provider, [], { save_fn: storage.save_fn, load_fn: storage.load_fn });

      await api.create_with_password(api.implicitSessionToken, "test_wallet", "pass123");

      await storage.syncToIdb();

      return await storage.listKeys();
    }, DB_NAME);

    expect(result).toStrictEqual(['test_wallet']);
  });

  test('Should not contain any data from the previous test in a new context', async ({ beekeeperWasmTestWebOnly }) => {
    const keys = await beekeeperWasmTestWebOnly(async ({ provider }, dbName) => {
      const storage = createIdbStorage(dbName);
      await storage.syncFromIdb();

      return await storage.listKeys();
    }, DB_NAME);

    // A fresh browser context has its own IndexedDB — should be empty
    expect(keys).toStrictEqual([]);
  });

  test('Should contain data from the previous test in the same context', async ({ beekeeperWasmTestWebOnlyWithPage }) => {
    const keys = await beekeeperWasmTestWebOnlyWithPage(page2, async ({ provider }, dbName) => {
      const storage = createIdbStorage(dbName);
      await storage.syncFromIdb();

      return await storage.listKeys();
    }, DB_NAME);

    // page2 shares context1 with page1 — should see the wallet from the first test
    expect(keys).toStrictEqual(['test_wallet']);
  });

  test('Should be able to init beekeeper and save the wallet file with explicitly closing the instance of beekeeper', async ({ beekeeperWasmTestWebOnlyWithPage }) => {
    const keys = await saveKeys(beekeeperWasmTestWebOnlyWithPage, page1, { close: true, dbName: DB_NAME });

    expect(keys).toStrictEqual(['test_wallet', 'w0']);
  });

  test('Should be able to init beekeeper and save the wallet file without explicitly closing the instance of beekeeper', async ({ beekeeperWasmTestWebOnlyWithPage }) => {
    const keys = await saveKeys(beekeeperWasmTestWebOnlyWithPage, page3, { close: false, dbName: DB_NAME });

    expect(keys).toStrictEqual(['w0']);
  });

  test('Should not be able to access previously created wallet from other context', async ({ beekeeperWasmTestWebOnly }) => {
    const hasNoWallets = await beekeeperWasmTestWebOnly(async ({ provider, BeekeeperInstanceHelper }, dbName) => {
      const storage = createIdbStorage(dbName);
      await storage.syncFromIdb();

      const api = new BeekeeperInstanceHelper(provider, [], { save_fn: storage.save_fn, load_fn: storage.load_fn });

      // In a fresh context there should be no wallets — opening "w0" should fail
      try {
        api.open(api.implicitSessionToken, "w0");
        await api.unlock(api.implicitSessionToken, "w0", "badf00d");
        return false; // Wallet found — not expected in a fresh context
      } catch {
        return true; // Expected — no wallet file exists
      }
    }, DB_NAME);

    expect(hasNoWallets).toBeTruthy();
  });

  test('Should be able to list the previously imported key from another page with the same browser context with explicitly closing the instance of beekeeper', async ({ beekeeperWasmTestWebOnlyWithPage }) => {
    const keys = await retrieveKeys(beekeeperWasmTestWebOnlyWithPage, page2, { close: true, dbName: DB_NAME });

    expect(keys).toStrictEqual({"keys": [{"public_key": "STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh"}]});
  });

  test('Should be able to list the previously imported key without explicitly closing the instance of beekeeper', async ({ beekeeperWasmTestWebOnlyWithPage }) => {
    const keys = await retrieveKeys(beekeeperWasmTestWebOnlyWithPage, page3, { close: false, dbName: DB_NAME });

    expect(keys).toStrictEqual({"keys": [{"public_key": "STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh"}]});
  });

  test.afterAll(async () => {
    await browser.close();
  });
});
