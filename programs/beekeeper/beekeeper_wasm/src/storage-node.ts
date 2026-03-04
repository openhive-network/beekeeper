/// <reference types="node" />

import { mkdirSync, readFileSync, writeFileSync, readdirSync } from 'node:fs';
import { join } from 'node:path';
import { type IStorageCallbacks, type TStorageCreator, LEGACY_WALLET_DIR, LEGACY_WALLET_EXT } from './detailed/fs.js';

const nodeStorageCreator: TStorageCreator = async (storageRoot: string): Promise<IStorageCallbacks> => {
  const walletDir = join(storageRoot, LEGACY_WALLET_DIR);
  mkdirSync(walletDir, { recursive: true });

  return {
    save_fn: (name: string, data: Uint8Array): void => {
      writeFileSync(join(walletDir, name + LEGACY_WALLET_EXT), data);
    },

    load_fn: (name: string): Uint8Array => {
      try {
        const buf = readFileSync(join(walletDir, name + LEGACY_WALLET_EXT));
        return new Uint8Array(buf);
      } catch {
        throw new Error("Wallet not found: " + name);
      }
    },

    list_dir_fn: (): string[] => {
      try {
        const files = readdirSync(walletDir);
        return files.map(f => f.slice(0, -LEGACY_WALLET_EXT.length));
      } catch {
        return [];
      }
    }
  };
};

export default nodeStorageCreator;
