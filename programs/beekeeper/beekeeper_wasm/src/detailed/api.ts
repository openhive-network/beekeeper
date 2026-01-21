/**
 * Beekeeper API - uses minimal WASM (secp256k1 only) + SubtleCrypto
 */

import { BeekeeperError } from "./errors.js";
import { IBeekeeperInstance, IBeekeeperOptions, IBeekeeperSession } from "./interfaces.js";
import { BeekeeperSession } from "./session.js";
import { loadMinimalWasm, MinimalWasmModule } from "./wasm/index.js";
import { createStorage, IStorage } from "./storage/index.js";
import { WalletManager } from "./core/wallet-manager.js";
import * as crypto from "./core/crypto.js";

export class BeekeeperApi implements IBeekeeperInstance {
  public readonly storage: IStorage | null;
  public readonly walletManager: WalletManager;
  public readonly wasm: MinimalWasmModule;

  public readonly sessions: Map<string, BeekeeperSession> = new Map();

  private unlockTimeout: number;
  private lastActivity: Date;
  private autoLockTimer: ReturnType<typeof setTimeout> | null = null;

  private constructor(
    wasm: MinimalWasmModule,
    storage: IStorage | null,
    options: Omit<IBeekeeperOptions, 'wasmLocation'>
  ) {
    this.wasm = wasm;
    this.storage = storage;
    this.walletManager = new WalletManager(wasm, storage);
    this.unlockTimeout = options.unlockTimeout;
    this.lastActivity = new Date();
  }

  /**
   * Create and initialize a BeekeeperApi instance
   */
  public static async create(options: IBeekeeperOptions): Promise<BeekeeperApi> {
    // Load minimal WASM module
    const wasm = await loadMinimalWasm(options.wasmLocation);

    // Create storage (or null for in-memory mode)
    const storage = options.inMemory ? null : createStorage(options.storageRoot, false);

    // Initialize storage
    if (storage) {
      await storage.init();
      await storage.mkdir('.beekeeper');
    }

    const api = new BeekeeperApi(wasm, storage, options);

    // Start auto-lock timer
    api.startAutoLockTimer();

    return api;
  }

  public getVersion(): string {
    return process.env.npm_package_version as string;
  }

  /**
   * Update last activity time (resets auto-lock timer)
   */
  public updateActivity(): void {
    this.lastActivity = new Date();
  }

  /**
   * Start the auto-lock timer
   */
  private startAutoLockTimer(): void {
    if (this.autoLockTimer) {
      clearInterval(this.autoLockTimer);
    }

    // Check every 10 seconds
    this.autoLockTimer = setInterval(() => {
      const elapsed = (Date.now() - this.lastActivity.getTime()) / 1000;
      if (elapsed >= this.unlockTimeout) {
        // Auto-lock all wallets
        for (const session of this.sessions.values()) {
          session.lockAll();
        }
      }
    }, 10000);
  }

  /**
   * Stop the auto-lock timer
   */
  private stopAutoLockTimer(): void {
    if (this.autoLockTimer) {
      clearInterval(this.autoLockTimer);
      this.autoLockTimer = null;
    }
  }

  /**
   * Get current info
   */
  public getInfo(): { now: Date; timeoutTime: Date } {
    this.updateActivity();
    const now = new Date();
    const timeoutTime = new Date(this.lastActivity.getTime() + this.unlockTimeout * 1000);
    return { now, timeoutTime };
  }

  public createSession(salt: string): IBeekeeperSession {
    this.updateActivity();

    // Generate session token using crypto random
    const tokenBytes = crypto.randomBytes(32);
    const saltBytes = crypto.stringToBytes(salt);

    // Combine random bytes with salt for token
    const combined = new Uint8Array(tokenBytes.length + saltBytes.length);
    combined.set(tokenBytes);
    combined.set(saltBytes, tokenBytes.length);

    const token = crypto.bytesToHex(tokenBytes);

    const session = new BeekeeperSession(this, token);
    this.sessions.set(token, session);

    return session;
  }

  public closeSession(token: string): void {
    this.updateActivity();

    if (!this.sessions.delete(token)) {
      throw new BeekeeperError(`This Beekeeper API instance is not the owner of session identified by token: "${token}"`);
    }
  }

  public async delete(): Promise<void> {
    this.stopAutoLockTimer();

    for (const session of this.sessions.values()) {
      session.close();
    }

    this.sessions.clear();
  }
}

/**
 * Default options for BeekeeperApi
 */
export const defaultBeekeeperOptions: Required<IBeekeeperOptions> = {
  storageRoot: typeof window !== 'undefined' ? '/storage_root' : './storage_root-node',
  wasmLocation: '',
  inMemory: false,
  enableLogs: false,
  unlockTimeout: 900
};

/**
 * Create a Beekeeper instance with optional configuration
 */
export async function createBeekeeper(options?: Partial<IBeekeeperOptions>): Promise<IBeekeeperInstance> {
  const mergedOptions: IBeekeeperOptions = {
    ...defaultBeekeeperOptions,
    ...options
  };

  return BeekeeperApi.create(mergedOptions);
}
