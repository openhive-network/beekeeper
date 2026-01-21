import { BeekeeperInstanceHelper, ExtractError } from './run_node_helper.js';

globalThis.createBeekeeperTestFor = async function createBeekeeperTestFor (env) {
  const locBeekeeper = env === 'web' ? '../../dist/bundle/web' : '../../dist/bundle/node';

  const storageRoot = env === "web" ? "/storage_root" : '.beekeeper';

  const beekeeper = await import(locBeekeeper);

  const bk = await beekeeper.default({ storageRoot, enableLogs: false });

  return {
    env,
    storageRoot,
    provider: beekeeper,
    beekeeper: bk
  };
};

globalThis.createBeekeeperWasmTestFor = async function createBeekeeperWasmTestFor (env) {
  // Import the JS loader, not the .wasm file directly
  const locBeekeeperWasm = env === 'web' ? '../../dist/bundle/build_minimal/beekeeper_minimal.js' : '../../dist/bundle/build_minimal/beekeeper_minimal.js';

  const wasm = await import(locBeekeeperWasm);

  const provider = await wasm.default();

  return {
    env,
    provider,
    ExtractError,
    BeekeeperInstanceHelper
  };
};

export {};
