import type { IStorageCallbacks, TStorageCreator } from './detailed/fs.js';

const openDb = (dbName: string): Promise<IDBDatabase> => new Promise<IDBDatabase>((resolve, reject) => {
  const req = indexedDB.open(dbName, 1);
  req.onupgradeneeded = () => req.result.createObjectStore('wallets');
  req.onsuccess = () => resolve(req.result);
  req.onerror = () => reject(req.error);
});

const webStorageCreator: TStorageCreator = async (dbName: string): Promise<IStorageCallbacks> => {
  return {
    save_fn: async (name: string, data: Uint8Array): Promise<void> => {
      const db = await openDb(dbName);
      const tx = db.transaction('wallets', 'readwrite');
      tx.objectStore('wallets').put(new Uint8Array(data), name);
      await new Promise<void>(r => { tx.oncomplete = () => r(); });
      db.close();
    },

    load_fn: async (name: string): Promise<Uint8Array> => {
      const db = await openDb(dbName);
      const tx = db.transaction('wallets', 'readonly');
      const result = await new Promise<Uint8Array | undefined>(r => {
        const req = tx.objectStore('wallets').get(name);
        req.onsuccess = () => r(req.result ? new Uint8Array(req.result) : undefined);
      });
      db.close();

      if (!result)
        throw new Error("Wallet not found: " + name);

      return result;
    },

    list_dir_fn: async (): Promise<string[]> => {
      const db = await openDb(dbName);
      const tx = db.transaction('wallets', 'readonly');
      const keys = await new Promise<string[]>(r => {
        const req = tx.objectStore('wallets').getAllKeys();
        req.onsuccess = () => r(req.result as string[]);
      });
      db.close();

      return keys;
    }
  };
};

export default webStorageCreator;