export interface IStorageCallbacks {
  save_fn: (name: string, data: Uint8Array) => void | Promise<void>;
  load_fn: (name: string) => Uint8Array | Promise<Uint8Array>;
  list_dir_fn: () => string[] | Promise<string[]>;
}

export type TStorageCreator = (storageBase: string) => Promise<IStorageCallbacks>;
