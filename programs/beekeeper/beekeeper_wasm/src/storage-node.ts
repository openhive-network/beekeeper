/// <reference types="node" />

import { mkdir, readFile, writeFile, readdir } from 'node:fs/promises';
import { join } from 'node:path';
import { type IStorageCallbacks, type TStorageCreator, LEGACY_WALLET_DIR, LEGACY_WALLET_EXT } from './detailed/fs.js';

const nodeStorageCreator: TStorageCreator = async (storageRoot: string): Promise<IStorageCallbacks> => {
  const walletDir = join(storageRoot, LEGACY_WALLET_DIR);
  await mkdir(walletDir, { recursive: true });

  return {
    save_fn: (name: string, data: Uint8Array): Promise<void> => {
      return writeFile(join(walletDir, name + LEGACY_WALLET_EXT), data);
    },

    load_fn: async (name: string): Promise<Uint8Array> => {
      try {
        const buf = await readFile(join(walletDir, name + LEGACY_WALLET_EXT));
        return new Uint8Array(buf);
      } catch {
        throw new Error("Wallet not found: " + name);
      }
    },

    list_dir_fn: async (): Promise<string[]> => {
      try {
        const files = await readdir(walletDir);
        return files.map(f => f.slice(0, -LEGACY_WALLET_EXT.length));
      } catch {
        return [];
      }
    }
  };
};

export default nodeStorageCreator;
