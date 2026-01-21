/**
 * Factory function to create a Beekeeper instance using minimal WASM
 */

import { BeekeeperApi } from "./api.js";
import { IBeekeeperInstance, IBeekeeperOptions } from "./interfaces.js";

const DEFAULT_BEEKEEPER_OPTIONS: Omit<IBeekeeperOptions, 'storageRoot' | 'wasmLocation'> = {
  enableLogs: false,
  unlockTimeout: 900,
  inMemory: false
};

interface IOptionalModuleArgs {
  wasmBinary?: Uint8Array;
  locateFile?: (path: string, scriptDirectory: string) => string;
}

/**
 * Create a Beekeeper instance
 *
 * @param beekeeperConstructor - Unused (kept for backward compatibility)
 * @param storageRoot - Root directory for wallet storage
 * @param ModuleExt - Module extension options (for WASM location)
 * @param isWebEnvironment - Whether running in browser
 * @param options - Additional options
 */
const createBeekeeper = async(
  _beekeeperConstructor: unknown,
  storageRoot: string,
  ModuleExt: IOptionalModuleArgs = {},
  _isWebEnvironment: boolean,
  options: Partial<IBeekeeperOptions> = {}
): Promise<IBeekeeperInstance> => {
  // Determine WASM location from ModuleExt if available
  let wasmLocation: string | undefined;
  if (ModuleExt.locateFile) {
    // Get the WASM file location
    wasmLocation = ModuleExt.locateFile('beekeeper_minimal.wasm', '');
  }

  const mergedOptions: IBeekeeperOptions = {
    ...DEFAULT_BEEKEEPER_OPTIONS,
    storageRoot,
    wasmLocation: wasmLocation || options.wasmLocation || '',
    ...options
  };

  return BeekeeperApi.create(mergedOptions);
};

export default createBeekeeper;
