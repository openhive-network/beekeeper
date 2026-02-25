import type Beekeeper from "../build/beekeeper_wasm.common";

import { BeekeeperApi } from "./api.js";
import type { IStorageCallbacks } from "./fs.js";
import { createInMemoryStorage } from "./fs.js";
import { IBeekeeperInstance, IBeekeeperOptions } from "./interfaces.js";
import { safeAsyncWasmCall } from "./util/wasm_error.js";

const DEFAULT_BEEKEEPER_OPTIONS: Omit<IBeekeeperOptions, 'wasmLocation' | 'storageRoot'> = {
  enableLogs: false,
  unlockTimeout: 900,
  inMemory: false
};

interface IOptionalModuleArgs {
  wasmBinary?: Uint8Array;
  locateFile?: (path: string, scriptDirectory: string) => string;
}

const createBeekeeper = async(
  beekeeperContstructor: typeof Beekeeper,
  ModuleExt: IOptionalModuleArgs = {},
  options: Partial<IBeekeeperOptions> = {}
): Promise<IBeekeeperInstance> => {
  const { wasmLocation: _wl, storageRoot: _sr, ...otherOptions } = options;
  const mergedOptions = { ...DEFAULT_BEEKEEPER_OPTIONS, ...otherOptions };

  const storage: IStorageCallbacks = mergedOptions.inMemory ? createInMemoryStorage() : (mergedOptions.storage ?? createInMemoryStorage());

  const beekeeperProvider = await safeAsyncWasmCall(() => beekeeperContstructor(ModuleExt), "Beekeeper WASM module loading");
  const api = new BeekeeperApi(beekeeperProvider, mergedOptions, storage);

  api.init();

  return api;
};

export default createBeekeeper;
