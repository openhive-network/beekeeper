export interface IStorageCallbacks {
  save_fn: (name: string, data: Uint8Array) => (Promise<void> | void);
  load_fn: (name: string) => (Promise<Uint8Array> | Uint8Array);
  list_dir_fn: () => (Promise<string[]> | string[]);
  close?: () => void;
}

export type TStorageCreator = (storageBase: string) => Promise<IStorageCallbacks>;
