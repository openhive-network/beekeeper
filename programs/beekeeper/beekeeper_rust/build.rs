use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let local_include = manifest_dir.join("include");
    let core_minimal_dir = manifest_dir.join("../core_minimal");
    let core_minimal_include = core_minimal_dir.join("include");
    let fc_crypto_bridge_dir = manifest_dir.join("../fc_crypto_bridge");
    let fc_crypto_bridge_include =
        env::var("BEEKEEPER_FC_CRYPTO_BRIDGE_INCLUDE_DIR")
            .map(PathBuf::from)
            .unwrap_or_else(|_| fc_crypto_bridge_dir.join("include"));

    let mut build = cxx_build::bridge("src/lib.rs");
    build
        .include(&local_include)
        .include(&core_minimal_include)
        .include(&fc_crypto_bridge_include)
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

    for src in ["cpp/rust_wallet_storage.cpp", "cpp/beekeeper_holder.cpp"] {
        let path = manifest_dir.join(src);
        if path.exists() {
            build.file(&path);
            println!("cargo:rerun-if-changed={src}");
        }
    }

    build.compile("beekeeper_rust_ffi");

    // Link the prebuilt fc_crypto_bridge static library and its transitive
    // deps (fc, secp256k1, OpenSSL, ...). The CMake build of beekeeper sets
    // BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR; BEEKEEPER_FC_LINK_FLAGS is a
    // shell-style "-Ldir -llib …" string that build.rs forwards to rustc.
    println!("cargo:rerun-if-env-changed=BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR");
    println!("cargo:rerun-if-env-changed=BEEKEEPER_FC_CRYPTO_BRIDGE_INCLUDE_DIR");
    println!("cargo:rerun-if-env-changed=BEEKEEPER_FC_LINK_FLAGS");

    if let Ok(dir) = env::var("BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR") {
        println!("cargo:rustc-link-search=native={dir}");
        println!("cargo:rustc-link-lib=static=fc_crypto_bridge");
    }
    if let Ok(flags) = env::var("BEEKEEPER_FC_LINK_FLAGS") {
        for tok in flags.split_whitespace() {
            if let Some(p) = tok.strip_prefix("-L") {
                println!("cargo:rustc-link-search=native={p}");
            } else if let Some(l) = tok.strip_prefix("-l") {
                println!("cargo:rustc-link-lib={l}");
            }
        }
    }

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!(
        "cargo:rerun-if-changed=include/beekeeper_rs/rust_wallet_storage.hpp"
    );
    println!(
        "cargo:rerun-if-changed=include/beekeeper_rs/beekeeper_holder.hpp"
    );
}
