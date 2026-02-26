/// <reference types="node" />

import { mkdir, readFile, writeFile, readdir } from 'node:fs/promises';
import { join } from 'node:path';
import type { IStorageCallbacks, TStorageCreator } from './detailed/fs.js';

const nodeStorageCreator: TStorageCreator = async (storageRoot: string): Promise<IStorageCallbacks> => {
  await mkdir(storageRoot, { recursive: true });

  return {
    save_fn: (name: string, data: Uint8Array): Promise<void> => {
      return writeFile(join(storageRoot, name), data);
    },

    load_fn: async (name: string): Promise<Uint8Array> => {
      try {
        const buf = await readFile(join(storageRoot, name));
        return new Uint8Array(buf);
      } catch {
        throw new Error("Wallet not found: " + name);
      }
    },

    list_dir_fn: async (): Promise<string[]> => {
      try {
        return await readdir(storageRoot);
      } catch {
        return [];
      }
    }
  };
};

export default nodeStorageCreator;
