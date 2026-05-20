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
mod crypto;
mod errors;
pub mod session;
pub mod wallet;

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
    pub struct WalletDetails {
        pub name: String,
        pub unlocked: bool,
    }

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

        fn cpp_sha256(
            self: &mut RustCryptoProtocol,
            data: &[u8],
            out: &mut [u8],
        );
        fn cpp_sha512(
            self: &mut RustCryptoProtocol,
            data: &[u8],
            out: &mut [u8],
        );
        fn cpp_ripemd160(
            self: &mut RustCryptoProtocol,
            data: &[u8],
            out: &mut [u8],
        );

        fn cpp_aes256_cbc_encrypt(
            self: &mut RustCryptoProtocol,
            key: &[u8],
            iv: &[u8],
            data: &[u8],
        ) -> Result<Vec<u8>>;
        fn cpp_aes256_cbc_decrypt(
            self: &mut RustCryptoProtocol,
            key: &[u8],
            iv: &[u8],
            data: &[u8],
        ) -> Result<Vec<u8>>;

        fn cpp_generate_private_key(
            self: &mut RustCryptoProtocol,
            out: &mut [u8],
        );
        fn cpp_get_public_key(
            self: &mut RustCryptoProtocol,
            privkey: &[u8],
            out: &mut [u8],
        ) -> Result<()>;
        fn cpp_sign_compact(
            self: &mut RustCryptoProtocol,
            privkey: &[u8],
            digest: &[u8],
            out: &mut [u8],
        ) -> Result<()>;
        fn cpp_ecdh_shared_secret(
            self: &mut RustCryptoProtocol,
            privkey: &[u8],
            pubkey: &[u8],
            out: &mut [u8],
        ) -> Result<()>;

        fn cpp_base58_encode(
            self: &mut RustCryptoProtocol,
            data: &[u8],
        ) -> String;
        fn cpp_base58_decode(
            self: &mut RustCryptoProtocol,
            s: &str,
        ) -> Result<Vec<u8>>;

        fn cpp_get_random_bytes(self: &mut RustCryptoProtocol, out: &mut [u8]);
    }

    unsafe extern "C++" {
        include!("beekeeper_rs/rust_crypto_primitives.hpp");
        include!("beekeeper_rs/rust_wallet_storage.hpp");
        include!("beekeeper_rs/beekeeper_holder.hpp");

        #[namespace = "beekeeper_rs"]
        #[cxx_name = "beekeeper_holder"]
        type BeekeeperHolder;

        #[namespace = "beekeeper_rs"]
        fn new_beekeeper_holder(
            crypto_impl: Box<RustCryptoProtocol>,
            storage_impl: Box<RustStorageProtocol>,
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
        fn set_timeout(self: Pin<&mut BeekeeperHolder>, seconds: u32);

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
    }
}
