/**
 * Type definitions for the minimal WASM module
 */

export interface MinimalWasmModule {
  /**
   * Parse WIF private key and return the corresponding public key
   */
  wif_to_public_key(wif: string, prefix: string): string;

  /**
   * Validate WIF format
   */
  validate_wif(wif: string): string;

  /**
   * Sign a digest with a private key
   */
  sign_digest(wif: string, digestHex: string): string;

  /**
   * Compute ECDH shared secret (for ECIES encryption)
   */
  get_shared_secret(wif: string, pubkeyStr: string, prefix: string): string;

  /**
   * Get raw private key bytes from WIF
   */
  get_private_key_bytes(wif: string): string;
}

export interface WasmResult<T> {
  result?: T;
  error?: string;
}

export interface PublicKeyResult {
  public_key: string;
}

export interface ValidateResult {
  valid: boolean;
}

export interface SignatureResult {
  signature: string;
}

export interface SharedSecretResult {
  shared_secret: string;
}

export interface PrivateKeyResult {
  private_key: string;
}
