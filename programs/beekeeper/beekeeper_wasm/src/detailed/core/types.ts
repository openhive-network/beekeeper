/**
 * Core types for the minimal beekeeper implementation
 */

/** Public key type (STM prefixed string) */
export type TPublicKey = string;

/** Signature type (hex string) */
export type TSignature = string;

/** WIF private key type */
export type TWif = string;

/**
 * Stored key data (encrypted in wallet file)
 */
export interface StoredKey {
  publicKey: TPublicKey;
  encryptedWif: string;  // Base64 encoded AES-encrypted WIF
}

/**
 * Wallet file format (JSON, then AES encrypted)
 */
export interface WalletFileData {
  version: 1;
  keys: StoredKey[];
}

/**
 * Unlocked key (in memory only, never persisted)
 */
export interface UnlockedKey {
  publicKey: TPublicKey;
  wif: TWif;
}

/**
 * Wallet state
 */
export interface WalletState {
  name: string;
  isTemporary: boolean;
  isUnlocked: boolean;
  unlockedKeys: Map<TPublicKey, UnlockedKey>;
  passwordHash: Uint8Array | null;  // SHA-512 of password, used for AES
}

/**
 * Session state
 */
export interface SessionState {
  token: string;
  wallets: Map<string, WalletState>;
  createdAt: Date;
}

/**
 * Minimal WASM module interface
 */
export interface IMinimalWasm {
  wif_to_public_key(wif: string, prefix: string): string;
  validate_wif(wif: string): string;
  sign_digest(wif: string, digestHex: string): string;
  get_shared_secret(wif: string, pubkeyStr: string, prefix: string): string;
  get_private_key_bytes(wif: string): string;
}

/**
 * WASM response types
 */
export interface WasmPublicKeyResponse {
  public_key?: string;
  error?: string;
}

export interface WasmValidateResponse {
  valid?: boolean;
  error?: string;
}

export interface WasmSignatureResponse {
  signature?: string;
  error?: string;
}

export interface WasmSharedSecretResponse {
  shared_secret?: string;
  error?: string;
}
