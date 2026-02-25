/**
 * Async cryptographic callbacks for the WASM beekeeper module.
 *
 * Only hash and AES operations remain as JS callbacks — secp256k1, RIPEMD-160,
 * and Base58 are compiled natively into the WASM binary.
 *
 * All methods return Promises (backed by SubtleCrypto). The C++ side unwraps
 * them via emscripten::val::await() with Asyncify enabled.
 */

/**
 * Interface matching the C++ wasm_crypto_primitives JS callback expectations.
 * Every method returns a Promise (SubtleCrypto is async-only).
 */
export interface ICryptoCallbacks {
  sha256(data: Uint8Array): Promise<Uint8Array>;
  sha512(data: Uint8Array): Promise<Uint8Array>;
  aes256CbcEncrypt(key: Uint8Array, iv: Uint8Array, data: Uint8Array): Promise<Uint8Array>;
  aes256CbcDecrypt(key: Uint8Array, iv: Uint8Array, data: Uint8Array): Promise<Uint8Array>;
}

// TS 5.x: Uint8Array<ArrayBufferLike> isn't assignable to BufferSource because
// ArrayBufferLike includes SharedArrayBuffer. We narrow the type at the boundary.
type Buf = Uint8Array<ArrayBuffer>;
const asBuf = (u: Uint8Array): Buf => u as Buf;

/**
 * Creates the async crypto callbacks object that gets passed to the WASM constructor.
 * Uses the Web Crypto API (SubtleCrypto) — available in browsers and Node.js 15+.
 */
export function createCryptoCallbacks(): ICryptoCallbacks {
  return {
    async sha256(data: Uint8Array): Promise<Uint8Array> {
      return new Uint8Array(await crypto.subtle.digest('SHA-256', asBuf(data)));
    },

    async sha512(data: Uint8Array): Promise<Uint8Array> {
      return new Uint8Array(await crypto.subtle.digest('SHA-512', asBuf(data)));
    },

    async aes256CbcEncrypt(key: Uint8Array, iv: Uint8Array, data: Uint8Array): Promise<Uint8Array> {
      // SubtleCrypto AES-CBC adds PKCS#7 padding — this matches FC's aes_encrypt()
      // which also uses OpenSSL with PKCS#7 padding enabled.
      const cryptoKey = await crypto.subtle.importKey('raw', asBuf(key), 'AES-CBC', false, ['encrypt']);
      return new Uint8Array(await crypto.subtle.encrypt({ name: 'AES-CBC', iv: asBuf(iv) }, cryptoKey, asBuf(data)));
    },

    async aes256CbcDecrypt(key: Uint8Array, iv: Uint8Array, data: Uint8Array): Promise<Uint8Array> {
      // SubtleCrypto AES-CBC removes PKCS#7 padding — matches FC's aes_decrypt().
      const cryptoKey = await crypto.subtle.importKey('raw', asBuf(key), 'AES-CBC', false, ['decrypt']);
      return new Uint8Array(await crypto.subtle.decrypt({ name: 'AES-CBC', iv: asBuf(iv) }, cryptoKey, asBuf(data)));
    }
  };
}
