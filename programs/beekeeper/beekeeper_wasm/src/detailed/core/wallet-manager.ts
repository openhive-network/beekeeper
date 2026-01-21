/**
 * Wallet manager - handles wallet encryption, storage, and key management
 * Uses minimal WASM for secp256k1 and SubtleCrypto for AES/SHA
 */

import type { IStorage } from '../storage/index.js';
import type {
  IMinimalWasm,
  WalletState,
  WalletFileData,
  StoredKey,
  UnlockedKey,
  TPublicKey,
  TWif,
  WasmPublicKeyResponse,
  WasmSignatureResponse,
  WasmSharedSecretResponse
} from './types.js';
import * as crypto from './crypto.js';
import { BeekeeperError } from '../errors.js';

const WALLET_DIR = '.beekeeper';
const WALLET_EXTENSION = '.wallet';
const WALLET_VERSION = 1;

/**
 * Parse JSON response from WASM, throw on error
 */
function parseWasmResult<T>(json: string): T {
  const result = JSON.parse(json);
  if (result.error) {
    throw new BeekeeperError(`WASM error: ${result.error}`);
  }
  return result as T;
}

/**
 * Core wallet manager - handles all wallet operations
 */
export class WalletManager {
  private readonly wasm: IMinimalWasm;
  private readonly storage: IStorage | null;
  private readonly prefix: string;

  constructor(wasm: IMinimalWasm, storage: IStorage | null, prefix: string = 'STM') {
    this.wasm = wasm;
    this.storage = storage;
    this.prefix = prefix;
  }

  /**
   * Get wallet file path
   */
  private getWalletPath(name: string): string {
    return `${WALLET_DIR}/${name}${WALLET_EXTENSION}`;
  }

  /**
   * Hash password to get encryption key (SHA-512)
   */
  private async hashPassword(password: string): Promise<Uint8Array> {
    return crypto.sha512(crypto.stringToBytes(password));
  }

  /**
   * Encrypt wallet data
   */
  private async encryptWalletData(data: WalletFileData, passwordHash: Uint8Array): Promise<Uint8Array> {
    const json = JSON.stringify(data);
    const plaintext = crypto.stringToBytes(json);
    return crypto.aesEncrypt(passwordHash, plaintext);
  }

  /**
   * Decrypt wallet data
   */
  private async decryptWalletData(encrypted: Uint8Array, passwordHash: Uint8Array): Promise<WalletFileData> {
    try {
      const decrypted = await crypto.aesDecrypt(passwordHash, encrypted);
      const json = crypto.bytesToString(decrypted);
      return JSON.parse(json) as WalletFileData;
    } catch {
      throw new BeekeeperError('Failed to decrypt wallet - incorrect password or corrupted data');
    }
  }

  /**
   * Encrypt a WIF key for storage
   */
  private async encryptWif(wif: TWif, passwordHash: Uint8Array): Promise<string> {
    const encrypted = await crypto.aesEncrypt(passwordHash, crypto.stringToBytes(wif));
    return crypto.base64Encode(encrypted);
  }

  /**
   * Decrypt a stored WIF key
   */
  private async decryptWif(encryptedWif: string, passwordHash: Uint8Array): Promise<TWif> {
    const encrypted = crypto.base64Decode(encryptedWif);
    const decrypted = await crypto.aesDecrypt(passwordHash, encrypted);
    return crypto.bytesToString(decrypted);
  }

  /**
   * Check if wallet exists
   */
  async walletExists(name: string): Promise<boolean> {
    if (!this.storage) return false;
    return this.storage.exists(this.getWalletPath(name));
  }

  /**
   * Create a new wallet
   */
  async createWallet(name: string, password: string, isTemporary: boolean): Promise<WalletState> {
    if (!isTemporary && !this.storage) {
      throw new BeekeeperError('Cannot create persistent wallet without storage');
    }

    if (!isTemporary && await this.walletExists(name)) {
      throw new BeekeeperError(`Wallet "${name}" already exists`);
    }

    const passwordHash = await this.hashPassword(password);

    // Create empty wallet data
    const walletData: WalletFileData = {
      version: WALLET_VERSION,
      keys: []
    };

    // Save to storage if not temporary
    if (!isTemporary && this.storage) {
      await this.storage.mkdir(WALLET_DIR);
      const encrypted = await this.encryptWalletData(walletData, passwordHash);
      await this.storage.write(this.getWalletPath(name), encrypted);
    }

    return {
      name,
      isTemporary,
      isUnlocked: true,
      unlockedKeys: new Map(),
      passwordHash
    };
  }

  /**
   * Open an existing wallet (locked state)
   */
  async openWallet(name: string): Promise<WalletState> {
    if (!this.storage) {
      throw new BeekeeperError('Cannot open wallet without storage');
    }

    if (!await this.walletExists(name)) {
      throw new BeekeeperError(`Wallet "${name}" does not exist`);
    }

    return {
      name,
      isTemporary: false,
      isUnlocked: false,
      unlockedKeys: new Map(),
      passwordHash: null
    };
  }

  /**
   * Unlock a wallet with password
   */
  async unlockWallet(wallet: WalletState, password: string): Promise<void> {
    if (wallet.isUnlocked) {
      return; // Already unlocked
    }

    if (!this.storage) {
      throw new BeekeeperError('Cannot unlock wallet without storage');
    }

    const passwordHash = await this.hashPassword(password);
    const encrypted = await this.storage.read(this.getWalletPath(wallet.name));

    if (!encrypted) {
      throw new BeekeeperError(`Wallet "${wallet.name}" not found`);
    }

    // Decrypt wallet data (will throw on wrong password)
    const walletData = await this.decryptWalletData(encrypted, passwordHash);

    // Decrypt all keys
    wallet.unlockedKeys.clear();
    for (const storedKey of walletData.keys) {
      const wif = await this.decryptWif(storedKey.encryptedWif, passwordHash);
      wallet.unlockedKeys.set(storedKey.publicKey, {
        publicKey: storedKey.publicKey,
        wif
      });
    }

    wallet.passwordHash = passwordHash;
    wallet.isUnlocked = true;
  }

  /**
   * Lock a wallet
   */
  lockWallet(wallet: WalletState): void {
    wallet.unlockedKeys.clear();
    wallet.passwordHash = null;
    wallet.isUnlocked = false;
  }

  /**
   * Close a wallet
   */
  closeWallet(wallet: WalletState): void {
    this.lockWallet(wallet);
  }

  /**
   * Import a WIF key into an unlocked wallet
   */
  async importKey(wallet: WalletState, wif: TWif): Promise<TPublicKey> {
    if (!wallet.isUnlocked || !wallet.passwordHash) {
      throw new BeekeeperError('Wallet must be unlocked to import keys');
    }

    // Validate WIF and get public key from WASM
    const result = parseWasmResult<WasmPublicKeyResponse>(
      this.wasm.wif_to_public_key(wif, this.prefix)
    );

    if (!result.public_key) {
      throw new BeekeeperError('Invalid WIF key');
    }

    const publicKey = result.public_key;

    // Check if key already exists
    if (wallet.unlockedKeys.has(publicKey)) {
      throw new BeekeeperError(`Key ${publicKey} already exists in wallet`);
    }

    // Add to unlocked keys
    wallet.unlockedKeys.set(publicKey, { publicKey, wif });

    // Save to storage if not temporary
    if (!wallet.isTemporary && this.storage) {
      await this.saveWallet(wallet);
    }

    return publicKey;
  }

  /**
   * Remove a key from wallet
   */
  async removeKey(wallet: WalletState, publicKey: TPublicKey): Promise<void> {
    if (!wallet.isUnlocked) {
      throw new BeekeeperError('Wallet must be unlocked to remove keys');
    }

    if (!wallet.unlockedKeys.delete(publicKey)) {
      throw new BeekeeperError(`Key ${publicKey} not found in wallet`);
    }

    // Save to storage if not temporary
    if (!wallet.isTemporary && this.storage) {
      await this.saveWallet(wallet);
    }
  }

  /**
   * Check if wallet has a matching private key
   */
  hasMatchingPrivateKey(wallet: WalletState, publicKey: TPublicKey): boolean {
    return wallet.unlockedKeys.has(publicKey);
  }

  /**
   * Get all public keys in wallet
   */
  getPublicKeys(wallet: WalletState): TPublicKey[] {
    if (!wallet.isUnlocked) {
      return [];
    }
    return Array.from(wallet.unlockedKeys.keys());
  }

  /**
   * Sign a digest with a key from the wallet
   */
  signDigest(wallet: WalletState, publicKey: TPublicKey, digest: string | Uint8Array): string {
    if (!wallet.isUnlocked) {
      throw new BeekeeperError('Wallet must be unlocked to sign');
    }

    const key = wallet.unlockedKeys.get(publicKey);
    if (!key) {
      throw new BeekeeperError(`Key ${publicKey} not found in wallet`);
    }

    // Convert Uint8Array to hex if needed
    const digestHex = digest instanceof Uint8Array
      ? crypto.bytesToHex(digest)
      : digest;

    const result = parseWasmResult<WasmSignatureResponse>(
      this.wasm.sign_digest(key.wif, digestHex)
    );

    if (!result.signature) {
      throw new BeekeeperError('Signing failed');
    }

    return result.signature;
  }

  /**
   * Encrypt data using ECIES (ECDH + AES)
   */
  async encryptData(
    wallet: WalletState,
    fromPublicKey: TPublicKey,
    toPublicKey: TPublicKey,
    content: string,
    nonce?: number
  ): Promise<string> {
    if (!wallet.isUnlocked) {
      throw new BeekeeperError('Wallet must be unlocked to encrypt');
    }

    const fromKey = wallet.unlockedKeys.get(fromPublicKey);
    if (!fromKey) {
      throw new BeekeeperError(`Key ${fromPublicKey} not found in wallet`);
    }

    // Get shared secret via ECDH
    const sharedSecretResult = parseWasmResult<WasmSharedSecretResponse>(
      this.wasm.get_shared_secret(fromKey.wif, toPublicKey, this.prefix)
    );

    if (!sharedSecretResult.shared_secret) {
      throw new BeekeeperError('Failed to compute shared secret');
    }

    // Use shared secret + nonce to derive encryption key
    const sharedSecret = crypto.hexToBytes(sharedSecretResult.shared_secret);
    const nonceBytes = nonce !== undefined
      ? crypto.stringToBytes(nonce.toString())
      : crypto.randomBytes(8);

    // Derive key: SHA-512(shared_secret || nonce)
    const keyMaterial = new Uint8Array(sharedSecret.length + nonceBytes.length);
    keyMaterial.set(sharedSecret);
    keyMaterial.set(nonceBytes, sharedSecret.length);
    const encryptionKey = await crypto.sha512(keyMaterial);

    // Encrypt content
    const contentBytes = crypto.stringToBytes(content);
    const encrypted = await crypto.aesEncrypt(encryptionKey, contentBytes);

    // Format: nonce_hex:encrypted_base64
    const nonceHex = crypto.bytesToHex(nonceBytes);
    const encryptedBase64 = crypto.base64Encode(encrypted);

    return `${nonceHex}:${encryptedBase64}`;
  }

  /**
   * Decrypt data using ECIES (ECDH + AES)
   */
  async decryptData(
    wallet: WalletState,
    fromPublicKey: TPublicKey,
    toPublicKey: TPublicKey,
    encryptedContent: string
  ): Promise<string> {
    if (!wallet.isUnlocked) {
      throw new BeekeeperError('Wallet must be unlocked to decrypt');
    }

    // Try to find a matching private key (either from or to)
    let privateKey: UnlockedKey | undefined;
    let otherPublicKey: TPublicKey = toPublicKey; // Default, will be overwritten

    if (wallet.unlockedKeys.has(fromPublicKey)) {
      privateKey = wallet.unlockedKeys.get(fromPublicKey);
      otherPublicKey = toPublicKey;
    } else if (wallet.unlockedKeys.has(toPublicKey)) {
      privateKey = wallet.unlockedKeys.get(toPublicKey);
      otherPublicKey = fromPublicKey;
    }

    if (!privateKey) {
      throw new BeekeeperError('No matching private key found for decryption');
    }

    // Parse encrypted content: nonce_hex:encrypted_base64
    const parts = encryptedContent.split(':');
    if (parts.length !== 2) {
      throw new BeekeeperError('Invalid encrypted content format');
    }

    const nonceHex = parts[0];
    const encryptedBase64 = parts[1];

    // Get shared secret via ECDH
    const sharedSecretResult = parseWasmResult<WasmSharedSecretResponse>(
      this.wasm.get_shared_secret(privateKey.wif, otherPublicKey, this.prefix)
    );

    if (!sharedSecretResult.shared_secret) {
      throw new BeekeeperError('Failed to compute shared secret');
    }

    // Derive key: SHA-512(shared_secret || nonce)
    const sharedSecret = crypto.hexToBytes(sharedSecretResult.shared_secret);
    const nonceBytes = crypto.hexToBytes(nonceHex);
    const keyMaterial = new Uint8Array(sharedSecret.length + nonceBytes.length);
    keyMaterial.set(sharedSecret);
    keyMaterial.set(nonceBytes, sharedSecret.length);
    const decryptionKey = await crypto.sha512(keyMaterial);

    // Decrypt content
    const encrypted = crypto.base64Decode(encryptedBase64);
    const decrypted = await crypto.aesDecrypt(decryptionKey, encrypted);

    return crypto.bytesToString(decrypted);
  }

  /**
   * Save wallet to storage
   */
  private async saveWallet(wallet: WalletState): Promise<void> {
    if (!this.storage || !wallet.passwordHash) {
      return;
    }

    // Build wallet data from unlocked keys
    const keys: StoredKey[] = [];
    for (const [publicKey, key] of wallet.unlockedKeys) {
      const encryptedWif = await this.encryptWif(key.wif, wallet.passwordHash);
      keys.push({ publicKey, encryptedWif });
    }

    const walletData: WalletFileData = {
      version: WALLET_VERSION,
      keys
    };

    const encrypted = await this.encryptWalletData(walletData, wallet.passwordHash);
    await this.storage.write(this.getWalletPath(wallet.name), encrypted);
  }
}
