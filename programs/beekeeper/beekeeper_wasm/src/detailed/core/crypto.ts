/**
 * Cryptographic utilities using Web Crypto API (SubtleCrypto)
 * Provides AES-256-CBC encryption and SHA hashing compatible with original FC implementation
 */

// Get crypto object (works in both browser and Node.js 18+)
function getCrypto(): Crypto {
  if (typeof globalThis.crypto !== 'undefined' && globalThis.crypto.subtle) {
    return globalThis.crypto;
  }
  throw new Error('Web Crypto API not available');
}

/**
 * Compute SHA-256 hash
 */
export async function sha256(data: Uint8Array): Promise<Uint8Array> {
  const hash = await getCrypto().subtle.digest('SHA-256', data);
  return new Uint8Array(hash);
}

/**
 * Compute SHA-512 hash
 */
export async function sha512(data: Uint8Array): Promise<Uint8Array> {
  const hash = await getCrypto().subtle.digest('SHA-512', data);
  return new Uint8Array(hash);
}

/**
 * AES-256-CBC encryption compatible with FC library.
 * Uses first 32 bytes of key as AES key, bytes 32-47 as IV.
 */
export async function aesEncrypt(key: Uint8Array, plaintext: Uint8Array): Promise<Uint8Array> {
  if (key.length < 48) {
    throw new Error('Key must be at least 48 bytes (32 for AES key + 16 for IV)');
  }

  const aesKey = key.slice(0, 32);
  const iv = key.slice(32, 48);

  const cryptoKey = await getCrypto().subtle.importKey(
    'raw',
    aesKey,
    { name: 'AES-CBC' },
    false,
    ['encrypt']
  );

  const ciphertext = await getCrypto().subtle.encrypt(
    { name: 'AES-CBC', iv },
    cryptoKey,
    plaintext
  );

  return new Uint8Array(ciphertext);
}

/**
 * AES-256-CBC decryption compatible with FC library.
 */
export async function aesDecrypt(key: Uint8Array, ciphertext: Uint8Array): Promise<Uint8Array> {
  if (key.length < 48) {
    throw new Error('Key must be at least 48 bytes (32 for AES key + 16 for IV)');
  }

  const aesKey = key.slice(0, 32);
  const iv = key.slice(32, 48);

  const cryptoKey = await getCrypto().subtle.importKey(
    'raw',
    aesKey,
    { name: 'AES-CBC' },
    false,
    ['decrypt']
  );

  const plaintext = await getCrypto().subtle.decrypt(
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
  const bytes = new Uint8Array(length);
  getCrypto().getRandomValues(bytes);
  return bytes;
}

/**
 * Generate a random hex token (for session tokens)
 */
export function generateToken(byteLength: number = 32): string {
  return bytesToHex(randomBytes(byteLength));
}

/**
 * Generate a secure random password
 */
export function generatePassword(length: number = 32): string {
  const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*';
  const randomValues = randomBytes(length);
  let result = '';
  for (let i = 0; i < length; i++) {
    result += chars[randomValues[i] % chars.length];
  }
  return result;
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

/**
 * Base64 encode
 */
export function base64Encode(bytes: Uint8Array): string {
  if (typeof btoa === 'function') {
    return btoa(String.fromCharCode(...bytes));
  }
  return Buffer.from(bytes).toString('base64');
}

/**
 * Base64 decode
 */
export function base64Decode(str: string): Uint8Array {
  if (typeof atob === 'function') {
    const binary = atob(str);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) {
      bytes[i] = binary.charCodeAt(i);
    }
    return bytes;
  }
  return new Uint8Array(Buffer.from(str, 'base64'));
}
