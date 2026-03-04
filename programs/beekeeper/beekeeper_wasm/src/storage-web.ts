import { type IStorageCallbacks, type TStorageCreator, LEGACY_WALLET_DIR, LEGACY_WALLET_EXT } from './detailed/fs.js';

const OBJECT_STORE_NAME = 'FILE_DATA' as const;

const openDb = (dbName: string): Promise<IDBDatabase> => new Promise<IDBDatabase>((resolve, reject) => {
  const req = indexedDB.open(dbName);
  req.onupgradeneeded = () => req.result.createObjectStore(OBJECT_STORE_NAME);
  req.onsuccess = () => {
    const db = req.result;
    if (!db.objectStoreNames.contains(OBJECT_STORE_NAME)) {
      db.close();
      const upgradeReq = indexedDB.open(dbName, db.version + 1);
      upgradeReq.onupgradeneeded = () => upgradeReq.result.createObjectStore(OBJECT_STORE_NAME);
      upgradeReq.onsuccess = () => resolve(upgradeReq.result);
      upgradeReq.onerror = () => reject(upgradeReq.error);
    } else {
      resolve(db);
    }
  };
  req.onerror = () => reject(req.error);
});

/** Load all wallet entries from IndexedDB into an in-memory Map. */
const preloadAll = (db: IDBDatabase, prefix: string): Promise<Map<string, Uint8Array>> =>
  new Promise((resolve, reject) => {
    const cache = new Map<string, Uint8Array>();
    const tx = db.transaction(OBJECT_STORE_NAME, 'readonly');
    const store = tx.objectStore(OBJECT_STORE_NAME);
    const req = store.openCursor();

    req.onsuccess = () => {
      const cursor = req.result;
      if (cursor) {
        const key = cursor.key as string;
        if (key.startsWith(prefix) && key.endsWith(LEGACY_WALLET_EXT))
          cache.set(key, new Uint8Array(cursor.value.contents));
        cursor.continue();
      }
    };

    tx.oncomplete = () => resolve(cache);
    tx.onerror = () => reject(tx.error);
  });

const webStorageCreator: TStorageCreator = async (dbName: string): Promise<IStorageCallbacks> => {
  const db = await openDb(dbName);
  const keyPrefix = `${dbName}/${LEGACY_WALLET_DIR}/`;

  // IDBFS-style: preload all wallet data into memory for synchronous reads
  const cache = await preloadAll(db, keyPrefix);

  const toKey = (name: string) => `${keyPrefix}${name}${LEGACY_WALLET_EXT}`;

  const pendingWrites: Promise<void>[] = [];

  return {
    save_fn: (name: string, data: Uint8Array): void => {
      const copy = new Uint8Array(data);
      cache.set(toKey(name), copy);

      // Fire-and-forget: persist to IndexedDB asynchronously
      const p = new Promise<void>((resolve, reject) => {
        const tx = db.transaction(OBJECT_STORE_NAME, 'readwrite');
        tx.objectStore(OBJECT_STORE_NAME).put({ contents: copy }, toKey(name));
        tx.oncomplete = () => resolve();
        tx.onerror = () => reject(tx.error);
      });
      pendingWrites.push(p);
      p.then(
        () => { pendingWrites.splice(pendingWrites.indexOf(p), 1); },
        () => { pendingWrites.splice(pendingWrites.indexOf(p), 1); }
      );
    },

    load_fn: (name: string): Uint8Array => {
      const data = cache.get(toKey(name));
      if (!data)
        throw new Error("Wallet not found: " + name);
      return data;
    },

    list_dir_fn: (): string[] => {
      const names: string[] = [];
      for (const key of cache.keys())
        names.push(key.slice(keyPrefix.length, -LEGACY_WALLET_EXT.length));
      return names;
    },

    sync: async (): Promise<void> => {
      await Promise.all(pendingWrites);
    },

    close: () => {
      db.close();
    }
  };
};

export default webStorageCreator;
