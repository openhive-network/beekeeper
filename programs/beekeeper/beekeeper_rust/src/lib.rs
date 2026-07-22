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
//! use beekeeper::prelude::*;
//!
//! let bk = BeekeeperApi::new(
//!     BeekeeperOptions::new("./storage_root-rust").unlock_timeout(900),
//! );
//! let session = bk.create_session().unwrap();
//! let mut created = session.create_wallet("my-wallet", "password").unwrap();
//! let pubkey = created.import_key("5J...wif...").unwrap();
//! let sig = created.sign_digest(&pubkey, "deadbeef...").unwrap();
//! // Dropping `session` locks its wallets and closes the session.
//! ```
//!
//! # Design notes
//!
//! The API follows the npm package, but where Rust allows a safer shape
//! than a 1:1 port, the Rust shape wins:
//!
//! - **Synchronous API.** All calls are blocking.
//! - **Sessions close on `Drop`.** A [`session::Session`] is an owned
//!   guard that locks its wallets and closes itself when it goes out of
//!   scope; there is no explicit `close`.
//! - **Lock state lives in the type system.** `unlock`/`lock`/`close`
//!   consume the wallet handle and return the next state, so stale
//!   handles are unrepresentable.
//! - **Fixed backends.** Storage is always the bundled filesystem-backed
//!   [`storage::RustStorageProtocol`] and crypto is always the FC-backed
//!   provider; in-memory mode is selected through
//!   [`BeekeeperOptions::in_memory`]. The key prefix is hard-coded to
//!   [`wallet::DEFAULT_KEY_PREFIX`] (`"STM"`).

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

/// One-line import for everything the README examples use.
///
/// ```no_run
/// use beekeeper::prelude::*;
/// ```
pub mod prelude {
    pub use crate::api::BeekeeperApi;
    pub use crate::errors::BeekeeperError;
    pub use crate::options::{BeekeeperOptions, DEFAULT_STORAGE_ROOT};
    pub use crate::session::{Session, SessionInfo};
    pub use crate::wallet::{
        CreatedWallet, DEFAULT_KEY_PREFIX, LockedWallet, UnlockedWallet,
        WalletInfo,
    };
}

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
