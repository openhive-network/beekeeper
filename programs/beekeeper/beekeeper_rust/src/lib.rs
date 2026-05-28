//! Rust bindings for the Beekeeper wallet daemon.
//!
//! This crate exposes the same wallet-management surface as the
//! `@hiveio/beekeeper` npm package (`programs/beekeeper/beekeeper_wasm`),
//! but as a native Rust library. The underlying wallet logic and crypto
//! primitives live in the shared C++ `core/` library and the FC-backed
//! `fc_crypto_bridge`, reached through a [`cxx`] bridge
//! ([`ffi::BeekeeperHolder`]). Storage callbacks are still injected from
//! Rust via [`RustStorageProtocol`].
//!
//! # Quick start
//!
//! ```no_run
//! use beekeeper_rust::{BeekeeperApi, BeekeeperOptions};
//!
//! let mut bk = BeekeeperApi::new(
//!     BeekeeperOptions::new("./storage_root-rust").unlock_timeout(900),
//! );
//! let token = bk.create_session().unwrap();
//! let created = bk.session(&token)
//!     .create_wallet("my-wallet", Some("password"), None)
//!     .unwrap();
//! let mut wallet = created.wallet;
//! let pubkey = wallet.import_key("5J...wif...").unwrap();
//! let sig = wallet.sign_digest(&pubkey, "deadbeef...").unwrap();
//! ```
//!
//! # Mapping to the TypeScript package
//!
//! Each module mirrors a TypeScript file under
//! `programs/beekeeper/beekeeper_wasm/src/detailed/`:
//!
//! | Rust module       | TypeScript file        |
//! |-------------------|------------------------|
//! | [`api`]           | `api.ts`               |
//! | [`session`]       | `session.ts`           |
//! | [`wallet`]        | `wallet.ts`            |
//! | [`options`]       | `interfaces.ts`        |
//! | [`BeekeeperError`] | `errors.ts`            |
//!
//! # Cross-language differences
//!
//! Behaviour matches the TypeScript binding wherever practical. The notable
//! divergences are:
//!
//! - **Synchronous API.** All Rust calls are blocking; TS marks
//!   `createWallet`, `unlock`, `importKey`, `removeKey`, `signDigest`,
//!   `encryptData`, `decryptData`, and `delete` as `async`/`Promise<...>`
//!   because WASM dispatches them on a worker.
//! - **No `salt` for `create_session`.** TS requires a salt string; the C++
//!   holder used here derives its own token, so the Rust signature drops the
//!   parameter.
//! - **No injectable storage / crypto callbacks.** TS lets callers supply
//!   `IStorageCallbacks` / `ICryptoCallbacks`; Rust always uses the bundled
//!   filesystem-backed [`storage::RustStorageProtocol`] and the FC-backed
//!   crypto provider linked from `fc_crypto_bridge`. In-memory mode is
//!   selected through [`BeekeeperOptions::in_memory`] instead.
//! - **Key-prefix is hard-coded to `"STM"`** in [`wallet::DEFAULT_KEY_PREFIX`].
//!   The C++ ABI still accepts a prefix, but the Rust facade doesn't expose
//!   it (TS doesn't either).
//! - **`sign_digest` takes a hex string only.** TS additionally accepts
//!   `Uint8Array`; Rust callers should hex-encode themselves.
//! - **Move-based wallet state machine.** `unlock`/`lock`/`close` consume
//!   the wallet handle and return the new state, leaning on Rust's ownership
//!   to prevent use of stale handles. TS keeps `BeekeeperLockedWallet`
//!   mutable and tracks the unlocked counterpart on an internal field.
//! - **`Session::list_wallets`** asks the C++ holder for every wallet in the
//!   session and recombines it with locally tracked `is_temporary` metadata.
//!   TS returns only the session's in-memory `wallets` map.
//! - **`get_version`** returns `CARGO_PKG_VERSION` instead of
//!   `npm_package_version`.

pub mod api;
pub mod options;
pub mod session;
pub mod wallet;

mod consts;
mod errors;
mod storage;

pub use api::BeekeeperApi;
pub use errors::BeekeeperError;
pub use options::BeekeeperOptions;
pub use storage::{RustStorageProtocol, new_rust_storage_protocol};

/// `cxx`-generated FFI between Rust and the C++ `beekeeper_rs::beekeeper_holder`.
///
/// Items in this module are produced by the [`cxx::bridge`] macro and are
/// considered an implementation detail of the crate. They are public so that
/// the higher-level wrappers in [`api`], [`session`] and [`wallet`] can hold
/// `UniquePtr<BeekeeperHolder>` fields, but new code outside of those modules
/// should prefer the safe wrappers.
///
/// All `cpp_*` methods on [`RustStorageProtocol`] are reachable from C++
/// only — they are listed here purely to satisfy the bridge contract.
#[cxx::bridge(namespace = "cpp")]
pub mod ffi {
    /// Plain-data summary of a wallet returned by C++ `list_wallets`.
    ///
    /// Equivalent of the per-entry record in `list_wallets` on the C++ side.
    /// The Rust facade widens this to [`wallet::WalletInfo`] with the extra
    /// `is_temporary` flag tracked by [`api::BeekeeperApi`].
    pub struct WalletDetails {
        pub name: String,
        pub unlocked: bool,
    }

    extern "Rust" {
        type RustStorageProtocol;

        fn new_rust_storage_protocol(
            storage_root: &str,
        ) -> Box<RustStorageProtocol>;

        fn cpp_load(
            self: &mut RustStorageProtocol,
            name: &str,
        ) -> Result<Vec<u8>>;

        fn cpp_save(
            self: &mut RustStorageProtocol,
            name: &str,
            data: &[u8],
        ) -> Result<()>;

        fn cpp_scan_dir(self: &mut RustStorageProtocol, name: &str) -> bool;
        fn cpp_sync(self: &mut RustStorageProtocol);
        fn cpp_close(self: &mut RustStorageProtocol);
    }

    unsafe extern "C++" {
        include!("beekeeper_rs/beekeeper_holder.hpp");
        include!("beekeeper_rs/rust_wallet_storage.hpp");

        #[namespace = "beekeeper_rs"]
        #[cxx_name = "beekeeper_holder"]
        type BeekeeperHolder;

        #[namespace = "beekeeper_rs"]
        fn new_beekeeper_holder(
            storage_impl: Box<RustStorageProtocol>,
            unlock_timeout_sec: u32,
        ) -> UniquePtr<BeekeeperHolder>;

        #[namespace = "beekeeper_rs"]
        fn new_beekeeper_holder_in_memory(
            unlock_timeout_sec: u32,
        ) -> UniquePtr<BeekeeperHolder>;

        fn create_session(self: Pin<&mut BeekeeperHolder>) -> Result<String>;
        fn close_session(
            self: Pin<&mut BeekeeperHolder>,
            token: &str,
        ) -> Result<()>;
        fn has_wallet(self: &BeekeeperHolder, name: &str) -> Result<bool>;
        fn create_wallet(
            self: Pin<&mut BeekeeperHolder>,
            token: &str,
            name: &str,
            password: &str,
            is_temporary: bool,
        ) -> Result<String>;
        fn open_wallet(
            self: Pin<&mut BeekeeperHolder>,
            token: &str,
            name: &str,
        ) -> Result<()>;
        fn close_wallet(
            self: Pin<&mut BeekeeperHolder>,
            name: &str,
        ) -> Result<()>;
        fn unlock(
            self: Pin<&mut BeekeeperHolder>,
            name: &str,
            password: &str,
        ) -> Result<()>;
        fn lock(self: Pin<&mut BeekeeperHolder>, name: &str) -> Result<()>;
        fn lock_all(self: Pin<&mut BeekeeperHolder>) -> Result<()>;
        fn sync_storage(self: Pin<&mut BeekeeperHolder>);
        fn close_storage(self: Pin<&mut BeekeeperHolder>);

        fn list_wallets(
            self: &BeekeeperHolder,
            token: &str,
        ) -> Result<Vec<WalletDetails>>;
        fn import_key(
            self: Pin<&mut BeekeeperHolder>,
            name: &str,
            wif_key: &str,
            prefix: &str,
        ) -> Result<String>;
        fn remove_key(
            self: Pin<&mut BeekeeperHolder>,
            name: &str,
            public_key: &str,
            prefix: &str,
        ) -> Result<()>;
        fn has_matching_private_key(
            self: &BeekeeperHolder,
            name: &str,
            public_key: &str,
            prefix: &str,
        ) -> Result<bool>;
        fn sign_digest(
            self: Pin<&mut BeekeeperHolder>,
            name: &str,
            digest_hex: &str,
            public_key: &str,
            prefix: &str,
        ) -> Result<String>;
        fn get_public_keys(
            self: &BeekeeperHolder,
            name: &str,
            prefix: &str,
        ) -> Result<Vec<String>>;
        fn encrypt_data(
            self: Pin<&mut BeekeeperHolder>,
            name: &str,
            from_key: &str,
            to_key: &str,
            content: &str,
            prefix: &str,
            nonce: u64,
        ) -> Result<String>;
        fn decrypt_data(
            self: Pin<&mut BeekeeperHolder>,
            name: &str,
            from_key: &str,
            to_key: &str,
            encrypted_content: &str,
            prefix: &str,
        ) -> Result<String>;
    }
}
