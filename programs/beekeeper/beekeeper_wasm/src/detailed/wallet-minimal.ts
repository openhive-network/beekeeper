/**
 * Minimal wallet implementation using:
 * - Thin WASM for secp256k1 operations
 * - SubtleCrypto for AES/SHA
 * - Abstract storage for persistence
 *
 * This is a reference implementation showing how the minimal architecture works.
 */

import { MinimalCrypto, type MinimalWasmFactory } from './wasm/index.js';
import { IStorage, createStorage } from './storage/index.js';
import * as crypto from './crypto/index.js';

/**
 * Wallet file format (encrypted with AES-256-CBC)
 */
interface WalletData {
  version: number;
  keys: Array<{
    publicKey: string;
    encryptedPrivateKey: string;  // AES encrypted WIF
  }>;
  checksum: string;  // SHA256 of password for verification
}

/**
 * Unlocked key data (in memory only)
 */
interface UnlockedKey {
  publicKey: string;
  wif: string;
}

/**
 * Minimal wallet manager implementation
 */
export class MinimalWalletManager {
  private readonly wasm: MinimalCrypto;
  private readonly storage: IStorage;
  private readonly prefix: string;

  // In-memory state
  private unlockedKeys: Map<string, UnlockedKey> = new Map();
  private passwordHash: Uint8Array | null = null;

  private constructor(
    wasm: MinimalCrypto,
    storage: IStorage,
    prefix: string
  ) {
    this.wasm = wasm;
    this.storage = storage;
    this.prefix = prefix;
  }

  /**
   * Create a new MinimalWalletManager
   */
  static async create(
    wasmFactory: MinimalWasmFactory,
    storageRoot: string,
    options: { inMemory?: boolean; prefix?: string } = {}
  ): Promise<MinimalWalletManager> {
    const prefix = options.prefix ?? 'STM';
    const wasm = await import('./wasm/index.js').then(m =>
      m.createMinimalCrypto(wasmFactory, prefix)
    );
    const storage = createStorage(storageRoot, options.inMemory ?? false);
    await storage.init();

    return new MinimalWalletManager(wasm, storage, prefix);
  }

  /**
   * Create a new wallet with a password
   */
  async createWallet(name: string, password: string): Promise<void> {
    const path = this.getWalletPath(name);

    if (await this.storage.exists(path)) {
      throw new Error(`Wallet "${name}" already exists`);
    }

    // Generate password hash for encryption/verification
    const passwordBytes = crypto.stringToBytes(password);
    const passwordHash = await crypto.sha512(passwordBytes);

    // Create empty wallet data
    const walletData: WalletData = {
      version: 1,
      keys: [],
      checksum: crypto.bytesToHex(await crypto.sha256(passwordHash))
    };

    // Encrypt and save
    const encrypted = await this.encryptWalletData(walletData, passwordHash);
    await this.storage.write(path, encrypted);
  }

  /**
   * Unlock a wallet with its password
   */
  async unlockWallet(name: string, password: string): Promise<void> {
    const path = this.getWalletPath(name);
    const encrypted = await this.storage.read(path);

    if (!encrypted) {
      throw new Error(`Wallet "${name}" not found`);
    }

    // Generate password hash
    const passwordBytes = crypto.stringToBytes(password);
    const passwordHash = await crypto.sha512(passwordBytes);

    // Decrypt wallet data
    const walletData = await this.decryptWalletData(encrypted, passwordHash);

    // Verify checksum
    const expectedChecksum = crypto.bytesToHex(await crypto.sha256(passwordHash));
    if (walletData.checksum !== expectedChecksum) {
      throw new Error('Invalid password');
    }

    // Store password hash and decrypt all keys
    this.passwordHash = passwordHash;

    for (const key of walletData.keys) {
      const wifBytes = await crypto.aesDecrypt(
        passwordHash,
        crypto.hexToBytes(key.encryptedPrivateKey)
      );
      const wif = crypto.bytesToString(wifBytes);

      this.unlockedKeys.set(key.publicKey, { publicKey: key.publicKey, wif });
    }
  }

  /**
   * Lock the wallet (clear in-memory keys)
   */
  lockWallet(): void {
    this.unlockedKeys.clear();
    this.passwordHash = null;
  }

  /**
   * Import a WIF private key
   */
  async importKey(name: string, wif: string): Promise<string> {
    if (!this.passwordHash) {
      throw new Error('Wallet not unlocked');
    }

    // Validate and get public key from WASM
    const publicKey = this.wasm.wifToPublicKey(wif);

    if (this.unlockedKeys.has(publicKey)) {
      throw new Error('Key already exists in wallet');
    }

    // Encrypt the WIF
    const wifBytes = crypto.stringToBytes(wif);
    const encryptedWif = await crypto.aesEncrypt(this.passwordHash, wifBytes);

    // Add to unlocked keys
    this.unlockedKeys.set(publicKey, { publicKey, wif });

    // Update wallet file
    await this.updateWalletFile(name, publicKey, crypto.bytesToHex(encryptedWif));

    return publicKey;
  }

  /**
   * Sign a digest with a key from the wallet
   */
  signDigest(publicKey: string, digest: string | Uint8Array): string {
    const key = this.unlockedKeys.get(publicKey);
    if (!key) {
      throw new Error(`Key ${publicKey} not found in unlocked wallet`);
    }

    return this.wasm.signDigest(key.wif, digest);
  }

  /**
   * Get ECDH shared secret for ECIES encryption
   */
  getSharedSecret(publicKey: string, otherPublicKey: string): string {
    const key = this.unlockedKeys.get(publicKey);
    if (!key) {
      throw new Error(`Key ${publicKey} not found in unlocked wallet`);
    }

    return this.wasm.getSharedSecret(key.wif, otherPublicKey);
  }

  /**
   * List all public keys in the unlocked wallet
   */
  getPublicKeys(): string[] {
    return Array.from(this.unlockedKeys.keys());
  }

  /**
   * Check if wallet is unlocked
   */
  isUnlocked(): boolean {
    return this.passwordHash !== null;
  }

  // Private helpers

  private getWalletPath(name: string): string {
    return `.beekeeper/${name}.wallet`;
  }

  private async encryptWalletData(data: WalletData, key: Uint8Array): Promise<Uint8Array> {
    const json = JSON.stringify(data);
    const bytes = crypto.stringToBytes(json);

    // Pad to block size (16 bytes for AES)
    const padded = this.padPKCS7(bytes);

    return crypto.aesEncrypt(key, padded);
  }

  private async decryptWalletData(encrypted: Uint8Array, key: Uint8Array): Promise<WalletData> {
    const decrypted = await crypto.aesDecrypt(key, encrypted);
    const unpadded = this.unpadPKCS7(decrypted);
    const json = crypto.bytesToString(unpadded);

    return JSON.parse(json);
  }

  private padPKCS7(data: Uint8Array): Uint8Array {
    const blockSize = 16;
    const padding = blockSize - (data.length % blockSize);
    const padded = new Uint8Array(data.length + padding);
    padded.set(data);
    padded.fill(padding, data.length);
    return padded;
  }

  private unpadPKCS7(data: Uint8Array): Uint8Array {
    if (data.length === 0) return data;
    const padding = data[data.length - 1];
    if (padding > 16 || padding === 0) return data;
    return data.slice(0, data.length - padding);
  }

  private async updateWalletFile(
    name: string,
    publicKey: string,
    encryptedWif: string
  ): Promise<void> {
    const path = this.getWalletPath(name);
    const encrypted = await this.storage.read(path);

    if (!encrypted || !this.passwordHash) {
      throw new Error('Wallet not found or not unlocked');
    }

    const walletData = await this.decryptWalletData(encrypted, this.passwordHash);

    walletData.keys.push({
      publicKey,
      encryptedPrivateKey: encryptedWif
    });

    const newEncrypted = await this.encryptWalletData(walletData, this.passwordHash);
    await this.storage.write(path, newEncrypted);
  }
}
