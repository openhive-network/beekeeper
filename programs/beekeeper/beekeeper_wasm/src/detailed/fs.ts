export interface IStorageCallbacks {
  save_fn: (name: string, data: Uint8Array) => void;
  load_fn: (name: string) => Uint8Array;
  list_dir_fn: () => string[];
  /** Flush pending writes to persistent storage (e.g. IndexedDB). No-op for synchronous backends. */
  sync?: () => Promise<void>;
  close?: () => void;
}

export const LEGACY_WALLET_EXT = '.wallet' as const;
export const LEGACY_WALLET_DIR = '.beekeeper' as const;

export type TStorageCreator = (storageBase: string) => Promise<IStorageCallbacks>;
