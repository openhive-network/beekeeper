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

const webStorageCreator: TStorageCreator = async (dbName: string): Promise<IStorageCallbacks> => {
  const db = await openDb(dbName);

  return {
    save_fn: async (name: string, data: Uint8Array): Promise<void> => {
      const tx = db.transaction(OBJECT_STORE_NAME, 'readwrite');
      tx.objectStore(OBJECT_STORE_NAME).put({
        contents: new Uint8Array(data)
      }, `${dbName}/${LEGACY_WALLET_DIR}/${name}${LEGACY_WALLET_EXT}`);
      await new Promise<void>(r => { tx.oncomplete = () => r(); });
    },

    load_fn: async (name: string): Promise<Uint8Array> => {
      const tx = db.transaction(OBJECT_STORE_NAME, 'readonly');
      const result = await new Promise<Uint8Array | undefined>(r => {
        const req = tx.objectStore(OBJECT_STORE_NAME).get(`${dbName}/${LEGACY_WALLET_DIR}/${name}${LEGACY_WALLET_EXT}`);
        req.onsuccess = () => r(req.result ? new Uint8Array(req.result.contents) : undefined);
      });

      if (!result)
        throw new Error("Wallet not found: " + name);

      return result;
    },

    list_dir_fn: async (): Promise<string[]> => {
      const tx = db.transaction(OBJECT_STORE_NAME, 'readonly');
      const keys = await new Promise<string[]>(r => {
        const req = tx.objectStore(OBJECT_STORE_NAME).getAllKeys();
        req.onsuccess = () => r(req.result as string[]);
      });

      return keys
        .filter(k => k.endsWith(LEGACY_WALLET_EXT))
        .map(k => k.slice(`${dbName}/${LEGACY_WALLET_DIR}/`.length, -LEGACY_WALLET_EXT.length));
    },

    close: () => {
      db.close();
    }
  };
};

export default webStorageCreator;
