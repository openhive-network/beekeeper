import type { IStorage } from './types.js';

/**
 * In-memory storage implementation.
 * Used for temporary wallets and testing.
 */
export class MemoryStorage implements IStorage {
  private files = new Map<string, Uint8Array>();

  public async init(): Promise<void> {
    // No initialization needed
  }

  public async read(path: string): Promise<Uint8Array | null> {
    return this.files.get(this.normalizePath(path)) ?? null;
  }

  public async write(path: string, data: Uint8Array): Promise<void> {
    this.files.set(this.normalizePath(path), data);
  }

  public async delete(path: string): Promise<void> {
    this.files.delete(this.normalizePath(path));
  }

  public async exists(path: string): Promise<boolean> {
    return this.files.has(this.normalizePath(path));
  }

  public async list(path: string): Promise<string[]> {
    const prefix = this.normalizePath(path);
    const results: string[] = [];

    for (const key of this.files.keys()) {
      if (key.startsWith(prefix + '/')) {
        const relative = key.slice(prefix.length + 1);
        const firstSegment = relative.split('/')[0];
        if (firstSegment && !results.includes(firstSegment)) {
          results.push(firstSegment);
        }
      }
    }

    return results;
  }

  public async mkdir(_path: string): Promise<void> {
    // No-op for memory storage
  }

  private normalizePath(path: string): string {
    return path.replace(/\/+/g, '/').replace(/^\/|\/$/g, '');
  }
}
