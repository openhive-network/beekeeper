export interface IStorageCallbacks {
  save_fn: (name: string, data: Uint8Array) => void;
  load_fn: (name: string) => Uint8Array;
}

export function createInMemoryStorage(): IStorageCallbacks & { store: Map<string, Uint8Array> } {
  const store = new Map<string, Uint8Array>();
  const save_fn = (name: string, data: Uint8Array) => { store.set(name, new Uint8Array(data)); };
  const load_fn = (name: string): Uint8Array => {
    const d = store.get(name);
    if (!d) throw new Error("Wallet not found: " + name);
    return d;
  };
  return { save_fn, load_fn, store };
}
