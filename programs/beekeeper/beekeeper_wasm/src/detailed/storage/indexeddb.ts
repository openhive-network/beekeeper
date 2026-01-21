import type { IStorage } from './types.js';

const DB_NAME = 'beekeeper_storage';
const STORE_NAME = 'files';
const DB_VERSION = 1;

/**
 * IndexedDB storage implementation for web browsers.
 */
export class IndexedDBStorage implements IStorage {
  private db: IDBDatabase | null = null;
  private readonly storageRoot: string;

  public constructor(storageRoot: string) {
    this.storageRoot = this.normalizePath(storageRoot);
  }

  public async init(): Promise<void> {
    if (this.db) return;

    this.db = await new Promise<IDBDatabase>((resolve, reject) => {
      const request = indexedDB.open(DB_NAME, DB_VERSION);

      request.onerror = () => reject(new Error(`IndexedDB open failed: ${request.error?.message}`));

      request.onsuccess = () => resolve(request.result);

      request.onupgradeneeded = () => {
        const db = request.result;
        if (!db.objectStoreNames.contains(STORE_NAME)) {
          db.createObjectStore(STORE_NAME);
        }
      };
    });
  }

  public async read(path: string): Promise<Uint8Array | null> {
    const key = this.getKey(path);
    return this.transaction('readonly', store => {
      return new Promise((resolve, reject) => {
        const request = store.get(key);
        request.onerror = () => reject(new Error(`Read failed: ${request.error?.message}`));
        request.onsuccess = () => resolve(request.result ?? null);
      });
    });
  }

  public async write(path: string, data: Uint8Array): Promise<void> {
    const key = this.getKey(path);
    return this.transaction('readwrite', store => {
      return new Promise((resolve, reject) => {
        const request = store.put(data, key);
        request.onerror = () => reject(new Error(`Write failed: ${request.error?.message}`));
        request.onsuccess = () => resolve();
      });
    });
  }

  public async delete(path: string): Promise<void> {
    const key = this.getKey(path);
    return this.transaction('readwrite', store => {
      return new Promise((resolve, reject) => {
        const request = store.delete(key);
        request.onerror = () => reject(new Error(`Delete failed: ${request.error?.message}`));
        request.onsuccess = () => resolve();
      });
    });
  }

  public async exists(path: string): Promise<boolean> {
    const data = await this.read(path);
    return data !== null;
  }

  public async list(path: string): Promise<string[]> {
    const prefix = this.getKey(path) + '/';
    return this.transaction('readonly', store => {
      return new Promise((resolve, reject) => {
        const results: string[] = [];
        const request = store.openCursor();

        request.onerror = () => reject(new Error(`List failed: ${request.error?.message}`));
        request.onsuccess = () => {
          const cursor = request.result;
          if (cursor) {
            const key = cursor.key as string;
            if (key.startsWith(prefix)) {
              const relative = key.slice(prefix.length);
              const firstSegment = relative.split('/')[0];
              if (firstSegment && !results.includes(firstSegment)) {
                results.push(firstSegment);
              }
            }
            cursor.continue();
          } else {
            resolve(results);
          }
        };
      });
    });
  }

  public async mkdir(_path: string): Promise<void> {
    // No-op for IndexedDB - directories are implicit
  }

  private getKey(path: string): string {
    return this.storageRoot + '/' + this.normalizePath(path);
  }

  private normalizePath(path: string): string {
    return path.replace(/\/+/g, '/').replace(/^\/|\/$/g, '');
  }

  private async transaction<T>(
    mode: IDBTransactionMode,
    operation: (store: IDBObjectStore) => Promise<T>
  ): Promise<T> {
    if (!this.db) {
      throw new Error('Storage not initialized');
    }
    const tx = this.db.transaction(STORE_NAME, mode);
    const store = tx.objectStore(STORE_NAME);
    return operation(store);
  }
}
