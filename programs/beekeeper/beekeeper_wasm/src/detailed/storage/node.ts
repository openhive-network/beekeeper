import type { IStorage } from './types.js';

// Dynamic import for Node.js fs module (not available in browsers)
let fsPromises: typeof import('fs/promises') | null = null;
let pathModule: typeof import('path') | null = null;

async function getFs() {
  if (!fsPromises) {
    fsPromises = await import('fs/promises');
  }
  return fsPromises;
}

async function getPath() {
  if (!pathModule) {
    pathModule = await import('path');
  }
  return pathModule;
}

/**
 * Node.js filesystem storage implementation.
 */
export class NodeStorage implements IStorage {
  private readonly storageRoot: string;

  public constructor(storageRoot: string) {
    this.storageRoot = storageRoot;
  }

  public async init(): Promise<void> {
    const fs = await getFs();
    await fs.mkdir(this.storageRoot, { recursive: true });
  }

  public async read(path: string): Promise<Uint8Array | null> {
    const fs = await getFs();
    const fullPath = await this.getFullPath(path);

    try {
      const data = await fs.readFile(fullPath);
      return new Uint8Array(data);
    } catch (err) {
      if ((err as NodeJS.ErrnoException).code === 'ENOENT') {
        return null;
      }
      throw err;
    }
  }

  public async write(path: string, data: Uint8Array): Promise<void> {
    const fs = await getFs();
    const fullPath = await this.getFullPath(path);

    // Ensure parent directory exists
    const p = await getPath();
    await fs.mkdir(p.dirname(fullPath), { recursive: true });

    await fs.writeFile(fullPath, data);
  }

  public async delete(path: string): Promise<void> {
    const fs = await getFs();
    const fullPath = await this.getFullPath(path);

    try {
      await fs.unlink(fullPath);
    } catch (err) {
      if ((err as NodeJS.ErrnoException).code !== 'ENOENT') {
        throw err;
      }
    }
  }

  public async exists(path: string): Promise<boolean> {
    const fs = await getFs();
    const fullPath = await this.getFullPath(path);

    try {
      await fs.access(fullPath);
      return true;
    } catch {
      return false;
    }
  }

  public async list(path: string): Promise<string[]> {
    const fs = await getFs();
    const fullPath = await this.getFullPath(path);

    try {
      return await fs.readdir(fullPath);
    } catch (err) {
      if ((err as NodeJS.ErrnoException).code === 'ENOENT') {
        return [];
      }
      throw err;
    }
  }

  public async mkdir(path: string): Promise<void> {
    const fs = await getFs();
    const fullPath = await this.getFullPath(path);
    await fs.mkdir(fullPath, { recursive: true });
  }

  private async getFullPath(path: string): Promise<string> {
    const p = await getPath();
    return p.join(this.storageRoot, path);
  }
}
