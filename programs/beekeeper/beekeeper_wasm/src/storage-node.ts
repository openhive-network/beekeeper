/// <reference types="node" />

import { mkdir, readFile, writeFile, readdir } from 'node:fs/promises';
import { join } from 'node:path';
import type { IStorageCallbacks, TStorageCreator } from './detailed/fs.js';

const WALLET_EXT = '.wallet';

const nodeStorageCreator: TStorageCreator = async (storageRoot: string): Promise<IStorageCallbacks> => {
  const walletDir = join(storageRoot, '.beekeeper');
  await mkdir(walletDir, { recursive: true });

  return {
    save_fn: (name: string, data: Uint8Array): Promise<void> => {
      return writeFile(join(walletDir, name + WALLET_EXT), data);
    },

    load_fn: async (name: string): Promise<Uint8Array> => {
      try {
        const buf = await readFile(join(walletDir, name + WALLET_EXT));
        return new Uint8Array(buf);
      } catch {
        throw new Error("Wallet not found: " + name);
      }
    },

    list_dir_fn: async (): Promise<string[]> => {
      try {
        const files = await readdir(walletDir);
        return files.map(f => f.slice(0, -WALLET_EXT.length));
      } catch {
        return [];
      }
    }
  };
};

export default nodeStorageCreator;
