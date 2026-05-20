//! Filesystem layout constants shared with the legacy C++ daemon.
//!
//! These match the values hard-coded in `core/beekeeper_wallet_manager.cpp`
//! so that wallets created by the native daemon and by this Rust crate are
//! mutually readable.

/// File-name suffix used for persisted wallet payloads.
pub const LEGACY_WALLET_EXT: &str = ".wallet";

/// Sub-directory of [`BeekeeperOptions::storage_root`](crate::options::BeekeeperOptions)
/// that holds the wallet files.
pub const LEGACY_WALLET_DIR: &str = ".beekeeper";
