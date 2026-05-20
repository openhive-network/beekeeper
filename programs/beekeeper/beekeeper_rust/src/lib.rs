use std::{
    fs,
    path::{Path, PathBuf},
};

use crate::{
    consts::{LEGACY_WALLET_DIR, LEGACY_WALLET_EXT},
    errors::{BeekeeperError, Res},
};

pub mod api;
mod consts;
mod errors;

pub struct RustCryptoProtocol;

pub struct RustStorageProtocol {
    wallet_dir: PathBuf,
}

fn new_rust_storage_protocol(storage_root: &str) -> Box<RustStorageProtocol> {
    let wallet_dir = Path::new(storage_root).join(LEGACY_WALLET_DIR);
    fs::create_dir_all(&wallet_dir).ok();
    Box::new(RustStorageProtocol { wallet_dir })
}

impl RustStorageProtocol {
    fn cpp_load(&mut self, name: &str) -> Res<Vec<u8>> {
        let path = self.wallet_dir.join(format!("{name}{LEGACY_WALLET_EXT}"));
        fs::read(path)
            .map_err(|_| BeekeeperError::WalletNotFound { name: name.into() })
    }

    fn cpp_save(&mut self, name: &str, data: &[u8]) -> Res<()> {
        let path = self.wallet_dir.join(format!("{name}{LEGACY_WALLET_EXT}"));
        fs::write(path, data).map_err(|_| BeekeeperError::WalletWriteFailed {
            name: name.into(),
        })
    }

    fn cpp_scan_dir(&mut self, name: &str) -> bool {
        self.wallet_dir
            .join(format!("{name}{LEGACY_WALLET_EXT}"))
            .is_file()
    }
}

#[cxx::bridge(namespace = "cpp")]
pub mod ffi {
    extern "Rust" {
        type RustCryptoProtocol;
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
    }

    unsafe extern "C++" {
        include!("beekeeper_rs/rust_crypto_provider.hpp");
        include!("beekeeper_rs/rust_wallet_storage.hpp");
        include!("beekeeper_rs/beekeeper_holder.hpp");

        #[namespace = "beekeeper_rs"]
        type beekeeper_holder;

        #[namespace = "beekeeper_rs"]
        fn new_beekeeper_holder(
            crypto_impl: Box<RustCryptoProtocol>,
            storage_impl: Box<RustStorageProtocol>,
            unlock_timeout_sec: u32,
        ) -> UniquePtr<beekeeper_holder>;

        fn create_session(self: Pin<&mut beekeeper_holder>) -> Result<String>;
        fn close_session(
            self: Pin<&mut beekeeper_holder>,
            token: &str,
        ) -> Result<()>;
        fn has_wallet(self: &beekeeper_holder, name: &str) -> Result<bool>;
        fn create_wallet(
            self: Pin<&mut beekeeper_holder>,
            token: &str,
            name: &str,
            password: &str,
            is_temporary: bool,
        ) -> Result<String>;
        fn open_wallet(
            self: Pin<&mut beekeeper_holder>,
            token: &str,
            name: &str,
        ) -> Result<()>;
        fn close_wallet(
            self: Pin<&mut beekeeper_holder>,
            name: &str,
        ) -> Result<()>;
        fn unlock(
            self: Pin<&mut beekeeper_holder>,
            name: &str,
            password: &str,
        ) -> Result<()>;
        fn lock_all(self: Pin<&mut beekeeper_holder>) -> Result<()>;
        fn set_timeout(self: Pin<&mut beekeeper_holder>, seconds: u32);
    }
}
