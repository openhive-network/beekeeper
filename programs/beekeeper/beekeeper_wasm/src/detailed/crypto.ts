/**
 * Async cryptographic callbacks for the WASM beekeeper module.
 *
 * Only hash, HMAC, KDF and AES operations remain as JS callbacks — secp256k1,
 * RIPEMD-160, and Base58 are compiled natively into the WASM binary.
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
  /** HMAC-SHA256 tag (32 bytes). Returns null on failure (never rejects). */
  hmacSha256(key: Uint8Array, data: Uint8Array): Promise<Uint8Array | null>;
  aes256CbcEncrypt(key: Uint8Array, iv: Uint8Array, data: Uint8Array): Promise<Uint8Array>;
  aes256CbcDecrypt(key: Uint8Array, iv: Uint8Array, data: Uint8Array): Promise<Uint8Array | null>;
  /** PBKDF2-HMAC-SHA512: derive dkLen bytes. Returns null on failure (never rejects). */
  pbkdf2HmacSha512(password: Uint8Array, salt: Uint8Array, iterations: number, dkLen: number): Promise<Uint8Array | null>;
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

    async hmacSha256(key: Uint8Array, data: Uint8Array): Promise<Uint8Array | null> {
      // Must never reject: Emscripten's Asyncify val::await() has no rejection
      // handler, so a rejected Promise aborts the WASM instance (see
      // aes256CbcDecrypt below). Return null and let C++ throw.
      try {
        const cryptoKey = await crypto.subtle.importKey('raw', key as Uint8Array<ArrayBuffer>, { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']);
        return new Uint8Array(await crypto.subtle.sign('HMAC', cryptoKey, data as Uint8Array<ArrayBuffer>));
      } catch {
        return null;
      }
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

    async pbkdf2HmacSha512(password: Uint8Array, salt: Uint8Array, iterations: number, dkLen: number): Promise<Uint8Array | null> {
      // Runs only at wallet unlock/save — never on the signing path.
      // Must never reject (see hmacSha256 above).
      try {
        // Some WebCrypto implementations reject zero-length raw keys. HMAC
        // zero-pads keys to the block size, so an empty PBKDF2 password is by
        // construction equivalent to a single zero byte — substitute it to
        // keep historical empty-password wallets working.
        const passwordKey = password.length === 0 ? new Uint8Array(1) : password;
        const baseKey = await crypto.subtle.importKey('raw', passwordKey as Uint8Array<ArrayBuffer>, 'PBKDF2', false, ['deriveBits']);
        const bits = await crypto.subtle.deriveBits(
          { name: 'PBKDF2', hash: 'SHA-512', salt: salt as Uint8Array<ArrayBuffer>, iterations },
          baseKey,
          dkLen * 8
        );
        return new Uint8Array(bits);
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
