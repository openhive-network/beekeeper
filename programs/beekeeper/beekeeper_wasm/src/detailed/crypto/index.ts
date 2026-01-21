/**
 * Cryptographic utilities using Web Crypto API (SubtleCrypto).
 * Compatible with FC library's AES and hash implementations.
 */

// Get crypto object (works in both browser and Node.js)
function getCrypto(): Crypto {
  if (typeof globalThis.crypto !== 'undefined') {
    return globalThis.crypto;
  }
  // Node.js environment
  throw new Error('Web Crypto API not available');
}

/**
 * Compute SHA-256 hash
 */
export async function sha256(data: Uint8Array): Promise<Uint8Array> {
  const crypto = getCrypto();
  const hash = await crypto.subtle.digest('SHA-256', data);
  return new Uint8Array(hash);
}

/**
 * Compute SHA-512 hash
 */
export async function sha512(data: Uint8Array): Promise<Uint8Array> {
  const crypto = getCrypto();
  const hash = await crypto.subtle.digest('SHA-512', data);
  return new Uint8Array(hash);
}

/**
 * AES-256-CBC encryption compatible with FC library.
 * Uses first 32 bytes of key as AES key, bytes 32-47 as IV.
 *
 * @param key - 64-byte key (e.g., SHA-512 hash of password)
 * @param plaintext - data to encrypt
 * @returns encrypted data with PKCS7 padding
 */
export async function aesEncrypt(key: Uint8Array, plaintext: Uint8Array): Promise<Uint8Array> {
  if (key.length < 48) {
    throw new Error('Key must be at least 48 bytes (32 for AES key + 16 for IV)');
  }

  const crypto = getCrypto();
  const aesKey = key.slice(0, 32);
  const iv = key.slice(32, 48);

  const cryptoKey = await crypto.subtle.importKey(
    'raw',
    aesKey,
    { name: 'AES-CBC' },
    false,
    ['encrypt']
  );

  const ciphertext = await crypto.subtle.encrypt(
    { name: 'AES-CBC', iv },
    cryptoKey,
    plaintext
  );

  return new Uint8Array(ciphertext);
}

/**
 * AES-256-CBC decryption compatible with FC library.
 * Uses first 32 bytes of key as AES key, bytes 32-47 as IV.
 *
 * @param key - 64-byte key (e.g., SHA-512 hash of password)
 * @param ciphertext - encrypted data with PKCS7 padding
 * @returns decrypted data
 */
export async function aesDecrypt(key: Uint8Array, ciphertext: Uint8Array): Promise<Uint8Array> {
  if (key.length < 48) {
    throw new Error('Key must be at least 48 bytes (32 for AES key + 16 for IV)');
  }

  const crypto = getCrypto();
  const aesKey = key.slice(0, 32);
  const iv = key.slice(32, 48);

  const cryptoKey = await crypto.subtle.importKey(
    'raw',
    aesKey,
    { name: 'AES-CBC' },
    false,
    ['decrypt']
  );

  const plaintext = await crypto.subtle.decrypt(
    { name: 'AES-CBC', iv },
    cryptoKey,
    ciphertext
  );

  return new Uint8Array(plaintext);
}

/**
 * Generate cryptographically secure random bytes
 */
export function randomBytes(length: number): Uint8Array {
  const crypto = getCrypto();
  const bytes = new Uint8Array(length);
  crypto.getRandomValues(bytes);
  return bytes;
}

/**
 * Generate a random hex token (for session tokens)
 * @param byteLength - number of random bytes (output will be 2x this in hex chars)
 */
export function generateToken(byteLength: number = 64): string {
  const bytes = randomBytes(byteLength);
  return bytesToHex(bytes);
}

/**
 * Convert bytes to hex string
 */
export function bytesToHex(bytes: Uint8Array): string {
  return Array.from(bytes)
    .map(b => b.toString(16).padStart(2, '0'))
    .join('');
}

/**
 * Convert hex string to bytes
 */
export function hexToBytes(hex: string): Uint8Array {
  if (hex.length % 2 !== 0) {
    throw new Error('Hex string must have even length');
  }
  const bytes = new Uint8Array(hex.length / 2);
  for (let i = 0; i < hex.length; i += 2) {
    bytes[i / 2] = parseInt(hex.substring(i, i + 2), 16);
  }
  return bytes;
}

/**
 * Encode string to UTF-8 bytes
 */
export function stringToBytes(str: string): Uint8Array {
  return new TextEncoder().encode(str);
}

/**
 * Decode UTF-8 bytes to string
 */
export function bytesToString(bytes: Uint8Array): string {
  return new TextDecoder().decode(bytes);
}
