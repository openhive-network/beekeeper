/**
 * Wrapper for the minimal WASM module providing a clean TypeScript API
 */

import type {
  MinimalWasmModule,
  PublicKeyResult,
  ValidateResult,
  SignatureResult,
  SharedSecretResult,
  PrivateKeyResult
} from './types.js';

export type { MinimalWasmModule } from './types.js';

export class WasmError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'WasmError';
  }
}

/**
 * Parse JSON result from WASM and throw on error
 */
function parseResult<T>(json: string): T {
  const result = JSON.parse(json);
  if (result.error) {
    throw new WasmError(result.error);
  }
  return result as T;
}

/**
 * High-level wrapper for the minimal WASM crypto operations
 */
export class MinimalCrypto {
  private readonly wasm: MinimalWasmModule;
  private readonly prefix: string;

  constructor(wasm: MinimalWasmModule, prefix: string = 'STM') {
    this.wasm = wasm;
    this.prefix = prefix;
  }

  /**
   * Import a WIF private key and return the public key
   */
  wifToPublicKey(wif: string): string {
    const result = parseResult<PublicKeyResult>(
      this.wasm.wif_to_public_key(wif, this.prefix)
    );
    return result.public_key;
  }

  /**
   * Validate if a string is a valid WIF private key
   */
  validateWif(wif: string): boolean {
    const result = parseResult<ValidateResult>(this.wasm.validate_wif(wif));
    return result.valid;
  }

  /**
   * Sign a 32-byte digest with a private key
   * @param wif - Private key in WIF format
   * @param digest - 32-byte digest as hex string or Uint8Array
   * @returns Compact signature (65 bytes) as hex string
   */
  signDigest(wif: string, digest: string | Uint8Array): string {
    const digestHex = typeof digest === 'string'
      ? digest
      : Array.from(digest).map(b => b.toString(16).padStart(2, '0')).join('');

    const result = parseResult<SignatureResult>(
      this.wasm.sign_digest(wif, digestHex)
    );
    return result.signature;
  }

  /**
   * Compute ECDH shared secret for ECIES encryption
   * @param wif - Private key in WIF format
   * @param publicKey - Public key with prefix
   * @returns 32-byte shared secret as hex string
   */
  getSharedSecret(wif: string, publicKey: string): string {
    const result = parseResult<SharedSecretResult>(
      this.wasm.get_shared_secret(wif, publicKey, this.prefix)
    );
    return result.shared_secret;
  }

  /**
   * Get raw private key bytes from WIF
   * @param wif - Private key in WIF format
   * @returns 32-byte private key as hex string
   */
  getPrivateKeyBytes(wif: string): string {
    const result = parseResult<PrivateKeyResult>(
      this.wasm.get_private_key_bytes(wif)
    );
    return result.private_key;
  }
}

/**
 * Factory type for loading the minimal WASM module
 */
export type MinimalWasmFactory = () => Promise<MinimalWasmModule>;

/**
 * Create a MinimalCrypto instance from a WASM module factory
 */
export async function createMinimalCrypto(
  factory: MinimalWasmFactory,
  prefix: string = 'STM'
): Promise<MinimalCrypto> {
  const wasm = await factory();
  return new MinimalCrypto(wasm, prefix);
}
