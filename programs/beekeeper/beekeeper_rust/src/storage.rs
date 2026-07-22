//! Filesystem-backed wallet storage used by the C++ wallet manager.
//!
//! The wallet manager calls into this struct over the `cxx` bridge
//! whenever it needs to load, persist, probe, sync, or close a wallet file.
//!
//! There is no extension point for other backends — the storage is always
//! this filesystem implementation, and in-memory mode is handled by
//! selecting a different *C++* storage backend
//! ([`ffi::new_beekeeper_holder_in_memory`](crate::ffi::new_beekeeper_holder_in_memory))
//! rather than by swapping this struct.

use std::{
    fs,
    path::{Path, PathBuf},
};

use crate::{
    consts::{LEGACY_WALLET_DIR, LEGACY_WALLET_EXT},
    errors::{BeekeeperError, Res},
};

/// Filesystem storage backend handed to C++ as a `Box<RustStorageProtocol>`.
///
/// Construct via [`new_rust_storage_protocol`].
pub struct RustStorageProtocol {
    wallet_dir: PathBuf,
}

/// Build a [`RustStorageProtocol`] rooted at `storage_root`.
///
/// Creates `<storage_root>/.beekeeper/` if missing (errors are swallowed —
/// the directory may already exist and `cpp_save` will surface any real
/// problem later). Free function (rather than `RustStorageProtocol::new`)
/// because the signature must match what the `cxx` bridge expects.
pub fn new_rust_storage_protocol(
    storage_root: &str,
) -> Box<RustStorageProtocol> {
    let wallet_dir = Path::new(storage_root).join(LEGACY_WALLET_DIR);
    fs::create_dir_all(&wallet_dir).ok();

    Box::new(RustStorageProtocol { wallet_dir })
}

impl RustStorageProtocol {
    pub(crate) fn cpp_load(&mut self, name: &str) -> Res<Vec<u8>> {
        let path = self.wallet_path(name);
        fs::read(path)
            .map_err(|_| BeekeeperError::WalletNotFound { name: name.into() })
    }

    pub(crate) fn cpp_save(&mut self, name: &str, data: &[u8]) -> Res<()> {
        let path = self.wallet_path(name);
        fs::write(path, data).map_err(|_| BeekeeperError::WalletWriteFailed {
            name: name.into(),
        })
    }

    pub(crate) fn cpp_scan_dir(&mut self, name: &str) -> bool {
        self.wallet_path(name).is_file()
    }

    pub(crate) fn cpp_sync(&mut self) {}

    pub(crate) fn cpp_close(&mut self) {}

    fn wallet_path(&self, name: &str) -> PathBuf {
        self.wallet_dir.join(format!("{name}{LEGACY_WALLET_EXT}"))
    }
}
