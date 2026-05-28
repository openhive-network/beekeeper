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

    // Link fc_crypto_bridge and its transitive deps (fc, secp256k1, OpenSSL, …).
    //
    // Two modes:
    //  * Orchestrated build — the outer beekeeper CMake build has already built
    //    fc_crypto_bridge and passes its location via BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR
    //    plus a shell-style "-Ldir -llib …" string in BEEKEEPER_FC_LINK_FLAGS.
    //  * Standalone build — a plain `cargo build`/`cargo test` with no env vars:
    //    build.rs drives CMake itself so beekeeper_rust is a self-contained crate.
    println!("cargo:rerun-if-env-changed=BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR");
    println!("cargo:rerun-if-env-changed=BEEKEEPER_FC_CRYPTO_BRIDGE_INCLUDE_DIR");
    println!("cargo:rerun-if-env-changed=BEEKEEPER_FC_LINK_FLAGS");

    if let Ok(dir) = env::var("BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR") {
        println!("cargo:rustc-link-search=native={dir}");
        println!("cargo:rustc-link-lib=static=fc_crypto_bridge");
        if let Ok(flags) = env::var("BEEKEEPER_FC_LINK_FLAGS") {
            for tok in flags.split_whitespace() {
                if let Some(p) = tok.strip_prefix("-L") {
                    println!("cargo:rustc-link-search=native={p}");
                } else if let Some(l) = tok.strip_prefix("-l") {
                    println!("cargo:rustc-link-lib={l}");
                }
            }
        }
    } else {
        link_fc_crypto_bridge_via_cmake(&manifest_dir);
    }

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!(
        "cargo:rerun-if-changed=include/beekeeper_rs/rust_wallet_storage.hpp"
    );
    println!(
        "cargo:rerun-if-changed=include/beekeeper_rs/beekeeper_holder.hpp"
    );
}

/// Build the beekeeper `fc_crypto_bridge` target (and its transitive fc /
/// secp256k1 deps) via CMake, then emit the link directives needed by the final
/// binary. Used when no orchestrating CMake build provides the prebuilt libs.
fn link_fc_crypto_bridge_via_cmake(manifest_dir: &std::path::Path) {
    // Minimal driver that builds only fc_crypto_bridge + its deps, avoiding the
    // beekeeper daemon's Boost requirements the CI image cannot satisfy.
    let driver_dir = manifest_dir.join("fc_bridge_cmake");
    println!("cargo:rerun-if-changed={}", driver_dir.join("CMakeLists.txt").display());

    let mut cfg = cmake::Config::new(&driver_dir);
    cfg.build_target("fc_crypto_bridge")
        .profile("Release")
        .define("CMAKE_POSITION_INDEPENDENT_CODE", "ON")
        // fc declares cmake_minimum_required(2.8.12), rejected by modern CMake
        // unless the policy floor is lowered.
        .define("CMAKE_POLICY_VERSION_MINIMUM", "3.5");

    for var in ["OPENSSL_ROOT_DIR", "OPENSSL_INCLUDE_DIR", "BOOST_ROOT"] {
        if let Ok(v) = env::var(var) {
            cfg.define(var, &v);
            println!("cargo:rerun-if-env-changed={var}");
        }
    }

    let build_dir = cfg.build().join("build");

    // fc_crypto_bridge provides the fc_crypto_provider symbols; fc / secp256k1
    // are its transitive deps. beekeeper_core_minimal is compiled directly into
    // beekeeper_rust_ffi above, so its CMake archive is intentionally skipped to
    // avoid duplicate-symbol errors. Order matters for static resolution
    // (high-level → low-level).
    for lib in ["fc_crypto_bridge", "fc", "secp256k1"] {
        let archive = find_archive(&build_dir, lib).unwrap_or_else(|| {
            panic!("lib{lib}.a not found under {}", build_dir.display())
        });
        let dir = archive.parent().expect("archive has parent dir");
        println!("cargo:rustc-link-search=native={}", dir.display());
        println!("cargo:rustc-link-lib=static={lib}");
    }
    for lib in ["secp256k1_precomputed", "equihash"] {
        if let Some(archive) = find_archive(&build_dir, lib) {
            let dir = archive.parent().expect("archive has parent dir");
            println!("cargo:rustc-link-search=native={}", dir.display());
            println!("cargo:rustc-link-lib=static={lib}");
        }
    }

    // Boost / OpenSSL live outside the CMake tree; add their lib dirs to the
    // search path, then link the dynamic system deps fc pulls in.
    for var in ["BOOST_ROOT", "OPENSSL_ROOT_DIR"] {
        let Ok(root) = env::var(var) else { continue };
        for sub in ["lib", "lib64"] {
            let dir = PathBuf::from(&root).join(sub);
            if dir.is_dir() {
                println!("cargo:rustc-link-search=native={}", dir.display());
            }
        }
    }
    for c in ["chrono", "context", "coroutine", "date_time", "filesystem", "system", "thread"] {
        println!("cargo:rustc-link-lib=boost_{c}");
    }
    for s in ["ssl", "crypto", "z", "bz2", "pthread", "rt", "dl"] {
        println!("cargo:rustc-link-lib={s}");
    }
}

/// Recursively locate `lib<name>.a` under `dir`.
fn find_archive(dir: &std::path::Path, name: &str) -> Option<PathBuf> {
    let target = format!("lib{name}.a");
    for entry in std::fs::read_dir(dir).ok()?.flatten() {
        let path = entry.path();
        if path.is_dir() {
            if let Some(found) = find_archive(&path, name) {
                return Some(found);
            }
        } else if path.file_name().and_then(|n| n.to_str()) == Some(target.as_str()) {
            return Some(path);
        }
    }
    None
}
