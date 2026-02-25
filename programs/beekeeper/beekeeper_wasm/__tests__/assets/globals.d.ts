// We only want to import types here!!
import type { MainModule } from '../../dist/build/beekeeper_wasm.common.js';
import type { IBeekeeperInstance } from '../../src/index.js';
import type * as BeekeeperModule from '../../src/index.js';
import type { BeekeeperInstanceHelper, ExtractError } from './run_node_helper.js';

export type TEnvType = 'web' | 'node';

export interface IBeekeeperGlobals {
  env: TEnvType;
  storageRoot: string;
  provider: typeof BeekeeperModule;
  beekeeper: IBeekeeperInstance;
}

export interface IBeekeeperWasmGlobals {
  env: TEnvType;
  provider: MainModule;
  ExtractError: typeof ExtractError;
  BeekeeperInstanceHelper: typeof BeekeeperInstanceHelper;
}

export interface IIdbStorage {
  save_fn: (name: string, data: Uint8Array) => void;
  load_fn: (name: string) => Uint8Array;
  syncToIdb: () => Promise<void>;
  syncFromIdb: () => Promise<void>;
  listKeys: () => Promise<string[]>;
  cache: Map<string, Uint8Array>;
}

declare global {
  function createBeekeeperTestFor (env: TEnvType): Promise<IBeekeeperGlobals>;
  function createBeekeeperWasmTestFor (env: TEnvType): Promise<IBeekeeperWasmGlobals>;
  function createIdbStorage (dbName: string): IIdbStorage;
}
