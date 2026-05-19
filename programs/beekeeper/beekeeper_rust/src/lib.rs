pub struct RustCryptoProtocol;
pub struct RustStorageProtocol;

#[cxx::bridge(namespace = "cpp")]
pub mod ffi {
    extern "Rust" {
        type RustCryptoProtocol;
        type RustStorageProtocol;
    }

    unsafe extern "C++" {
        include!("beekeeper_rs/rust_crypto_provider.hpp");
        include!("beekeeper_rs/rust_wallet_storage.hpp");
    }
}
