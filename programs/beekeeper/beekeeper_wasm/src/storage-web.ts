import type { IStorageCallbacks, TStorageCreator } from './detailed/fs.js';

const WALLET_EXT = '.wallet';
const LEGACY_IDBFS_DB = '/storage_root_v3';

const openDb = (dbName: string): Promise<IDBDatabase> => new Promise<IDBDatabase>((resolve, reject) => {
  const req = indexedDB.open(dbName, 1);
  req.onupgradeneeded = () => req.result.createObjectStore('wallets');
  req.onsuccess = () => resolve(req.result);
  req.onerror = () => reject(req.error);
});

/**
 * One-time migration from the old Emscripten IDBFS format.
 *
 * IDBFS stored files in a `FILE_DATA` object store with full-path keys
 * (e.g. `/storage_root_v3/.beekeeper/name.wallet`) and values of
 * `{ timestamp, mode, contents: { 0: byte, 1: byte, ... } }`.
 *
 * Opens the known legacy DB directly (O(1) — no database enumeration),
 * extracts `.wallet` entries, copies them into the new clean `wallets` store,
 * and deletes the legacy DB.
 */
const migrateFromIDBFS = async (targetDb: IDBDatabase): Promise<void> => {
  const idbfsDb = await new Promise<IDBDatabase | null>(resolve => {
    const req = indexedDB.open(LEGACY_IDBFS_DB);
    req.onsuccess = () => {
      const db = req.result;
      if (db.objectStoreNames.contains('FILE_DATA'))
        resolve(db);
      else {
        db.close();
        // Clean up empty DB we just created by opening a non-existent name
        indexedDB.deleteDatabase(LEGACY_IDBFS_DB);
        resolve(null);
      }
    };
    req.onerror = () => resolve(null);
  });

  if (!idbfsDb) return;

  // Extract wallet entries from FILE_DATA
  const tx = idbfsDb.transaction('FILE_DATA', 'readonly');
  const store = tx.objectStore('FILE_DATA');

  const allKeys = await new Promise<IDBValidKey[]>(r => {
    const req = store.getAllKeys();
    req.onsuccess = () => r(req.result);
  });

  const walletKeys = allKeys.filter(
    k => typeof k === 'string' && k.endsWith(WALLET_EXT)
  ) as string[];

  const wallets: [string, Uint8Array][] = [];

  for (const fullPath of walletKeys) {
    const entry = await new Promise<any>(r => {
      const req = store.get(fullPath);
      req.onsuccess = () => r(req.result);
    });

    if (!entry?.contents) continue;

    // IDBFS contents is an object with numeric keys, not a typed array
    const contents = entry.contents;
    const length = Object.keys(contents).length;
    const bytes = new Uint8Array(length);
    for (let i = 0; i < length; i++)
      bytes[i] = contents[i];

    // Extract just the filename (e.g. "name.wallet") from the full path
    const fileName = fullPath.split('/').pop()!;
    wallets.push([fileName, bytes]);
  }

  idbfsDb.close();

  if (wallets.length === 0) {
    indexedDB.deleteDatabase(LEGACY_IDBFS_DB);
    return;
  }

  // Copy wallets into our clean DB
  const writeTx = targetDb.transaction('wallets', 'readwrite');
  const writeStore = writeTx.objectStore('wallets');
  for (const [key, data] of wallets)
    writeStore.put(data, key);
  await new Promise<void>(r => { writeTx.oncomplete = () => r(); });

  // Delete old IDBFS database after successful migration
  await new Promise<void>((resolve, reject) => {
    const req = indexedDB.deleteDatabase(LEGACY_IDBFS_DB);
    req.onsuccess = () => resolve();
    req.onerror = () => reject(req.error);
  });
};

const webStorageCreator: TStorageCreator = async (dbName: string): Promise<IStorageCallbacks> => {
  const db = await openDb(dbName);

  // One-time migration from old Emscripten IDBFS format
  await migrateFromIDBFS(db);

  return {
    save_fn: async (name: string, data: Uint8Array): Promise<void> => {
      const tx = db.transaction('wallets', 'readwrite');
      tx.objectStore('wallets').put(new Uint8Array(data), name + WALLET_EXT);
      await new Promise<void>(r => { tx.oncomplete = () => r(); });
    },

    load_fn: async (name: string): Promise<Uint8Array> => {
      const tx = db.transaction('wallets', 'readonly');
      const result = await new Promise<Uint8Array | undefined>(r => {
        const req = tx.objectStore('wallets').get(name + WALLET_EXT);
        req.onsuccess = () => r(req.result ? new Uint8Array(req.result) : undefined);
      });

      if (!result)
        throw new Error("Wallet not found: " + name);

      return result;
    },

    list_dir_fn: async (): Promise<string[]> => {
      const tx = db.transaction('wallets', 'readonly');
      const keys = await new Promise<string[]>(r => {
        const req = tx.objectStore('wallets').getAllKeys();
        req.onsuccess = () => r(req.result as string[]);
      });

      return keys.map(k => k.slice(0, -WALLET_EXT.length));
    },

    close: () => {
      db.close();
    }
  };
};

export default webStorageCreator;
