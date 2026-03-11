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
  aes256CbcDecrypt(key: Uint8Array, iv: Uint8Array, data: Uint8Array): Promise<Uint8Array | null>;
  /** Fill the provided WASM memory view with cryptographically secure random bytes (in place). */
  getRandomBytes(dest: Uint8Array): void;
}

/**
 * Creates the async crypto callbacks object that gets passed to the WASM constructor.
 * Uses the Web Crypto API (SubtleCrypto) — available in browsers and Node.js 15+.
 */
export function createCryptoCallbacks(): ICryptoCallbacks {
  return {
    async sha256(data: Uint8Array): Promise<Uint8Array> {
      return new Uint8Array(await crypto.subtle.digest('SHA-256', data as Uint8Array<ArrayBuffer>));
    },

    async sha512(data: Uint8Array): Promise<Uint8Array> {
      return new Uint8Array(await crypto.subtle.digest('SHA-512', data as Uint8Array<ArrayBuffer>));
    },

    async aes256CbcEncrypt(key: Uint8Array, iv: Uint8Array, data: Uint8Array): Promise<Uint8Array> {
      // SubtleCrypto AES-CBC adds PKCS#7 padding — this matches FC's aes_encrypt()
      // which also uses OpenSSL with PKCS#7 padding enabled.
      const cryptoKey = await crypto.subtle.importKey('raw', key as Uint8Array<ArrayBuffer>, 'AES-CBC', false, ['encrypt']);
      return new Uint8Array(await crypto.subtle.encrypt({ name: 'AES-CBC', iv: iv as Uint8Array<ArrayBuffer> }, cryptoKey, data as Uint8Array<ArrayBuffer>));
    },

    async aes256CbcDecrypt(key: Uint8Array, iv: Uint8Array, data: Uint8Array): Promise<Uint8Array | null> {
      // SubtleCrypto AES-CBC removes PKCS#7 padding — matches FC's aes_decrypt().
      // On wrong password, SubtleCrypto.decrypt() rejects with OperationError (bad padding).
      // We MUST catch here because Emscripten's Asyncify-based val::await() has no rejection
      // handler (_emval_await in libemval.js uses bare `await` with no try/catch). A rejected
      // Promise becomes an unhandled JS exception that aborts the WASM instance — C++ catch(...)
      // cannot intercept it. Return null so the C++ side can throw a proper std::runtime_error.
      try {
        const cryptoKey = await crypto.subtle.importKey('raw', key as Uint8Array<ArrayBuffer>, 'AES-CBC', false, ['decrypt']);
        return new Uint8Array(await crypto.subtle.decrypt({ name: 'AES-CBC', iv: iv as Uint8Array<ArrayBuffer> }, cryptoKey, data as Uint8Array<ArrayBuffer>));
      } catch {
        return null;
      }
    },

    getRandomBytes(dest: Uint8Array): void {
      // TS 5.x: Uint8Array<ArrayBufferLike> isn't assignable to BufferSource because
      // ArrayBufferLike includes SharedArrayBuffer. We narrow the type at the boundary.
      crypto.getRandomValues(dest as Uint8Array<ArrayBuffer>);
    }
  };
}
