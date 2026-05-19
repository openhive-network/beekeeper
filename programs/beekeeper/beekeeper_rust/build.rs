use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let local_include = manifest_dir.join("include");
    let core_minimal_dir = manifest_dir.join("../core_minimal");
    let core_minimal_include = core_minimal_dir.join("include");

    let mut build = cxx_build::bridge("src/lib.rs");
    build
        .include(&local_include)
        .include(&core_minimal_include)
        .std("c++17");

    for src in [
        "wallet.cpp",
        "beekeeper.cpp",
        "crypto_provider_impl.cpp",
        "memory_storage.cpp",
    ] {
        let path = core_minimal_dir.join(src);
        build.file(&path);
        println!("cargo:rerun-if-changed={}", path.display());
    }

    for src in [
        "cpp/rust_crypto_provider.cpp",
        "cpp/rust_wallet_storage.cpp",
    ] {
        let path = manifest_dir.join(src);
        if path.exists() {
            build.file(&path);
            println!("cargo:rerun-if-changed={src}");
        }
    }

    build.compile("beekeeper_rust_ffi");

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!("cargo:rerun-if-changed=include/beekeeper_rs/rust_crypto_provider.hpp");
    println!("cargo:rerun-if-changed=include/beekeeper_rs/rust_wallet_storage.hpp");
}
