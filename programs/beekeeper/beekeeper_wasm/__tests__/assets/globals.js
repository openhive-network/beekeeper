import { BeekeeperInstanceHelper, ExtractError } from './run_node_helper.js';

globalThis.createBeekeeperTestFor = async function createBeekeeperTestFor (env) {
  const locBeekeeper = env === 'web' ? '../../dist/bundle/web' : '../../dist/bundle/node';

  const beekeeper = await import(locBeekeeper);

  const bk = await beekeeper.default({ enableLogs: false });

  return {
    env,
    provider: beekeeper,
    beekeeper: bk
  };
};

globalThis.createBeekeeperWasmTestFor = async function createBeekeeperWasmTestFor (env) {
  const locBeekeeperWasm = env === 'web' ? '../../dist/bundle/build/beekeeper_wasm.web' : '../../dist/bundle/build/beekeeper_wasm.node';

  const wasm = await import(locBeekeeperWasm);

  const provider = await wasm.default();

  return {
    env,
    provider,
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

  return { save_fn, load_fn, syncToIdb, syncFromIdb, listKeys, cache };
};

export {};
