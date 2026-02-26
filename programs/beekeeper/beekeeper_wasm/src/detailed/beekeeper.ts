import type Beekeeper from "../build/beekeeper_wasm.common";

import { BeekeeperApi } from "./api.js";
import { createCryptoCallbacks } from "./crypto.js";
import type { IStorageCallbacks } from "./fs.js";
import { IBeekeeperInstance, IBeekeeperOptions } from "./interfaces.js";
import { safeAsyncWasmCall } from "./util/wasm_error.js";

interface IOptionalModuleArgs {
  wasmBinary?: Uint8Array;
  locateFile?: (path: string, scriptDirectory: string) => string;
}

const createBeekeeper = async(
  beekeeperContstructor: typeof Beekeeper,
  ModuleExt: IOptionalModuleArgs = {},
  options: Omit<IBeekeeperOptions, 'wasmLocation' | 'storageRoot'>,
  storage?: IStorageCallbacks
): Promise<IBeekeeperInstance> => {
  const crypto = createCryptoCallbacks();

  // When inMemory mode is active, no persistent storage is needed.
  // C++ still requires a valid storage object for js_callback_storage construction,
  // but temporary wallets will use the C++ in-memory store (mem_storage_) instead.
  storage ??= {
    save_fn: () => {},
    load_fn: (name) => { throw new Error("No persistent storage: " + name); },
    list_dir_fn: () => []
  };

  const beekeeperProvider = await safeAsyncWasmCall(() => beekeeperContstructor(ModuleExt), "Beekeeper WASM module loading");
  const api = new BeekeeperApi(beekeeperProvider, options, storage, crypto);

  return api;
};

export default createBeekeeper;
