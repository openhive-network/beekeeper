// We only want to import types here!!
import type { MainModule } from '../../dist/build/beekeeper_wasm.common.js';
import type { IBeekeeperInstance } from '../../src/index.js';
import type * as BeekeeperModule from '../../src/index.js';
import type { BeekeeperInstanceHelper } from './run_node_helper.js';
import type { ICryptoCallbacks } from '../../src/detailed/crypto.js';

export type TEnvType = 'web' | 'node';

export interface IBeekeeperGlobals {
  env: TEnvType;
  provider: typeof BeekeeperModule;
  beekeeper: IBeekeeperInstance;
}

export interface IBeekeeperWasmGlobals {
  env: TEnvType;
  provider: MainModule;
  cryptoCallbacks: ICryptoCallbacks;
  BeekeeperInstanceHelper: typeof BeekeeperInstanceHelper;
}

export interface IIdbStorage {
  save_fn: (name: string, data: Uint8Array) => Promise<void>;
  load_fn: (name: string) => Promise<Uint8Array>;
  list_dir_fn: () => Promise<string[]>;
  syncToIdb: () => Promise<void>;
  syncFromIdb: () => Promise<void>;
  listKeys: () => Promise<string[]>;
  close: () => void;
}

declare global {
  function createBeekeeperTestFor (env: TEnvType): Promise<IBeekeeperGlobals>;
  function createBeekeeperWasmTestFor (env: TEnvType): Promise<IBeekeeperWasmGlobals>;
  function createIdbStorage (dbName: string): Promise<IIdbStorage>;
}
