export type { IStorage, StorageFactory } from './types.js';
export { MemoryStorage } from './memory.js';
export { IndexedDBStorage } from './indexeddb.js';
export { NodeStorage } from './node.js';

import type { IStorage } from './types.js';
import { MemoryStorage } from './memory.js';
import { IndexedDBStorage } from './indexeddb.js';
import { NodeStorage } from './node.js';

/**
 * Detect environment and create appropriate storage
 */
export function createStorage(storageRoot: string, inMemory: boolean): IStorage {
  if (inMemory) {
    return new MemoryStorage();
  }

  // Check for browser environment (IndexedDB)
  if (typeof indexedDB !== 'undefined') {
    return new IndexedDBStorage(storageRoot);
  }

  // Assume Node.js environment
  return new NodeStorage(storageRoot);
}
