use std::{
    fs,
    path::{Path, PathBuf},
};

use crate::{
    consts::{LEGACY_WALLET_DIR, LEGACY_WALLET_EXT},
    errors::{BeekeeperError, Res},
};

pub struct RustStorageProtocol {
    pub(crate) wallet_dir: PathBuf,
}

pub fn new_rust_storage_protocol(
    storage_root: &str,
) -> Box<RustStorageProtocol> {
    let wallet_dir = Path::new(storage_root).join(LEGACY_WALLET_DIR);
    fs::create_dir_all(&wallet_dir).ok();

    Box::new(RustStorageProtocol { wallet_dir })
}

impl RustStorageProtocol {
    pub(crate) fn cpp_load(&mut self, name: &str) -> Res<Vec<u8>> {
        let path = self.wallet_dir.join(format!("{name}{LEGACY_WALLET_EXT}"));
        fs::read(path)
            .map_err(|_| BeekeeperError::WalletNotFound { name: name.into() })
    }

    pub(crate) fn cpp_save(&mut self, name: &str, data: &[u8]) -> Res<()> {
        let path = self.wallet_dir.join(format!("{name}{LEGACY_WALLET_EXT}"));
        fs::write(path, data).map_err(|_| BeekeeperError::WalletWriteFailed {
            name: name.into(),
        })
    }

    pub(crate) fn cpp_scan_dir(&mut self, name: &str) -> bool {
        self.wallet_dir
            .join(format!("{name}{LEGACY_WALLET_EXT}"))
            .is_file()
    }

    pub(crate) fn cpp_sync(&mut self) {}

    pub(crate) fn cpp_close(&mut self) {}
}
