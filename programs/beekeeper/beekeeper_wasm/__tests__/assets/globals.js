import { BeekeeperInstanceHelper } from './run_node_helper.js';

async function loadCryptoCallbacks() {
  const detailedModule = await import('../../dist/bundle/detailed/index.js');
  return detailedModule.createCryptoCallbacks();
}

async function loadStorageCallbacks(dbName) {
  const detailedModule = await import('../../dist/bundle/detailed/index.js');
  return detailedModule.createWebStorage(dbName);
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
    BeekeeperInstanceHelper
  };
};

/**
 * Creates IndexedDB-backed save/load callbacks for beekeeper_api
 * using the production storage-web implementation.
 */
globalThis.createIdbStorage = async function createIdbStorage(dbName) {
  const storage = await loadStorageCallbacks(dbName);

  return {
    save_fn: storage.save_fn,
    load_fn: storage.load_fn,
    list_dir_fn: storage.list_dir_fn,
    syncToIdb: () => storage.sync ? storage.sync() : Promise.resolve(),
    syncFromIdb: () => Promise.resolve(),
    listKeys: () => Promise.resolve(storage.list_dir_fn()),
    close: () => { storage.close?.(); }
  };
};

export {};
