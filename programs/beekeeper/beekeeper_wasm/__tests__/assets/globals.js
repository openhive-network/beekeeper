import { BeekeeperInstanceHelper, ExtractError } from './run_node_helper.js';

async function loadCryptoCallbacks() {
  const detailedModule = await import('../../dist/bundle/detailed/index.js');
  return detailedModule.createCryptoCallbacks();
}

globalThis.createBeekeeperTestFor = async function createBeekeeperTestFor (env) {
  const locBeekeeper = env === 'web' ? '../../dist/bundle/web' : '../../dist/bundle/node';

  const beekeeper = await import(locBeekeeper);

  const bk = await beekeeper.default();

  return {
    env,
    provider: beekeeper,
    beekeeper: bk
  };
};

globalThis.createBeekeeperWasmTestFor = async function createBeekeeperWasmTestFor (env) {
  const wasm = await import('../../dist/bundle/build/beekeeper_wasm.common');

  const provider = await wasm.default();
  const cryptoCallbacks = await loadCryptoCallbacks();

  // Set crypto callbacks on the helper so all tests automatically use the 4-arg constructor
  BeekeeperInstanceHelper.cryptoCallbacks = cryptoCallbacks;

  return {
    env,
    provider,
    cryptoCallbacks,
    ExtractError,
    BeekeeperInstanceHelper
  };
};

/**
 * Creates IndexedDB-backed save/load callbacks for beekeeper_api.
 * The cache Map mirrors IndexedDB; syncToIdb/syncFromIdb flush/populate it.
 */
globalThis.createIdbStorage = function createIdbStorage(dbName) {
  const cache = new Map();

  function openDb() {
    return new Promise((resolve, reject) => {
      const req = indexedDB.open(dbName, 1);
      req.onupgradeneeded = () => req.result.createObjectStore('wallets');
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => reject(req.error);
    });
  }

  async function syncToIdb() {
    const db = await openDb();
    const tx = db.transaction('wallets', 'readwrite');
    const store = tx.objectStore('wallets');
    for (const [k, v] of cache) store.put(v, k);
    await new Promise(r => { tx.oncomplete = r; });
    db.close();
  }

  async function syncFromIdb() {
    const db = await openDb();
    const tx = db.transaction('wallets', 'readonly');
    const store = tx.objectStore('wallets');
    const keys = await new Promise(r => { const req = store.getAllKeys(); req.onsuccess = () => r(req.result); });
    for (const k of keys) {
      const val = await new Promise(r => { const req = store.get(k); req.onsuccess = () => r(req.result); });
      cache.set(k, new Uint8Array(val));
    }
    db.close();
  }

  async function listKeys() {
    const db = await openDb();
    const tx = db.transaction('wallets', 'readonly');
    const store = tx.objectStore('wallets');
    const keys = await new Promise(r => { const req = store.getAllKeys(); req.onsuccess = () => r(req.result); });
    db.close();
    return keys;
  }

  const save_fn = (name, data) => { cache.set(name, new Uint8Array(data)); };
  const load_fn = (name) => {
    const d = cache.get(name);
    if (!d) throw new Error("Wallet not found: " + name);
    return d;
  };
  const list_dir_fn = () => Array.from(cache.keys());

  return { save_fn, load_fn, list_dir_fn, syncToIdb, syncFromIdb, listKeys, cache };
};

export {};
