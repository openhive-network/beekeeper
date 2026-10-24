/// <reference types="node" />

import { closeSync, fsyncSync, mkdirSync, openSync, readFileSync, readdirSync, renameSync, rmSync, writeSync } from 'node:fs';
import { join } from 'node:path';
import { type IStorageCallbacks, type TStorageCreator, LEGACY_WALLET_DIR, LEGACY_WALLET_EXT } from './detailed/fs.js';

const nodeStorageCreator: TStorageCreator = async (storageRoot: string): Promise<IStorageCallbacks> => {
  const walletDir = join(storageRoot, LEGACY_WALLET_DIR);
  mkdirSync(walletDir, { recursive: true });

  return {
    save_fn: (name: string, data: Uint8Array): void => {
      // Write a new owner-only temp file, flush it and rename it over the wallet, so an
      // interrupted save cannot truncate the only copy of the keys (unlocking a legacy
      // wallet rewrites it). 'wx' never follows a file or symlink left at the temp path.
      const target = join(walletDir, name + LEGACY_WALLET_EXT);
      const tmp = target + '.tmp';
      rmSync(tmp, { force: true });

      const fd = openSync(tmp, 'wx', 0o600);
      try {
        for (let written = 0; written < data.length; )
          written += writeSync(fd, data, written, data.length - written);
        fsyncSync(fd);
      } catch (error) {
        closeSync(fd);
        rmSync(tmp, { force: true });
        throw error;
      }
      closeSync(fd);

      renameSync(tmp, target);
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
        return readdirSync(walletDir)
          .filter(f => f.endsWith(LEGACY_WALLET_EXT))
          .map(f => f.slice(0, -LEGACY_WALLET_EXT.length));
      } catch {
        return [];
      }
    }
  };
};

export default nodeStorageCreator;
