use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

fn main() {
    // Runtime lookup, not the env! macro: cargo can reuse the compiled build
    // script across crate copies (e.g. `cargo package` verify builds), so a
    // compile-time path would leak from one into the other.
    let manifest_dir = PathBuf::from(
        env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR set by cargo"),
    );

    // Three modes:
    //  * Prebuilt (default) — link the prelinked native bundle at
    //    lib/libbeekeeper_native.a; needs no C++ toolchain, CMake, Boost or
    //    OpenSSL. This is what crate consumers (and the published package)
    //    build.
    //  * Standalone from-source — BEEKEEPER_FROM_SOURCE=1: compile the C++
    //    from the beekeeper tree and prelink it into a bundle; only set by
    //    prelink_bundle.sh to (re)generate lib/libbeekeeper_native.a.
    //  * Orchestrated — the outer beekeeper CMake build has already built
    //    fc_crypto_bridge and passes its location via
    //    BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR (+ BEEKEEPER_FC_LINK_FLAGS).
    println!("cargo:rerun-if-env-changed=BEEKEEPER_FROM_SOURCE");
    println!("cargo:rerun-if-env-changed=BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR");

    if env::var_os("BEEKEEPER_FROM_SOURCE").is_none()
        && env::var_os("BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR").is_none()
    {
        link_prebuilt(&manifest_dir);
        return;
    }

    build_from_source(&manifest_dir);
}

fn link_prebuilt(manifest_dir: &Path) {
    let lib_dir = manifest_dir.join("lib");
    let bundle = lib_dir.join("libbeekeeper_native.a");
    assert!(
        bundle.is_file(),
        "prebuilt bundle missing: {}; run prelink_bundle.sh first",
        bundle.display()
    );

    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=static=beekeeper_native");
    // The bundle statically contains fc, secp256k1, Boost, OpenSSL, zlib and
    // bz2; only the system C++ runtime stays dynamic.
    println!("cargo:rustc-link-lib=stdc++");
    println!("cargo:rerun-if-changed={}", bundle.display());
}

fn build_from_source(manifest_dir: &Path) {
    let local_include = manifest_dir.join("include");
    let core_minimal_dir = manifest_dir.join("../core_minimal");
    let core_minimal_include = core_minimal_dir.join("include");
    let fc_crypto_bridge_dir = manifest_dir.join("../fc_crypto_bridge");
    let fc_crypto_bridge_include =
        env::var("BEEKEEPER_FC_CRYPTO_BRIDGE_INCLUDE_DIR")
            .map(PathBuf::from)
            .unwrap_or_else(|_| fc_crypto_bridge_dir.join("include"));

    // The C++ shims include the generated bridge header as
    // "beekeeper/src/lib.rs.h"; pin the prefix so it doesn't follow the
    // crates.io package name (hiveio-beekeeper).
    cxx_build::CFG.include_prefix = "beekeeper";
    let mut build = cxx_build::bridge("src/lib.rs");
    build
        .include(&local_include)
        .include(&core_minimal_include)
        .include(&fc_crypto_bridge_include)
        .std("c++17")
        // The native code ships prebuilt inside the published crate, so
        // optimize for size rather than speed (crates.io enforces a 10 MB
        // package limit).
        .opt_level_str("s")
        // STB_GNU_UNIQUE symbols (guard variables, inline-function statics)
        // cannot be localized by objcopy and would collide with other
        // prelinked bundles (e.g. wax's) at final link.
        .flag_if_supported("-fno-gnu-unique");

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

    // The FFI archive is folded into the prelinked bundle below (standalone
    // mode) or linked explicitly (orchestrated mode), so cc must not emit its
    // own link directives.
    build.cargo_metadata(false);
    build.compile("beekeeper_rust_ffi");

    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR set by cargo"));
    let ffi_archive = out_dir.join("libbeekeeper_rust_ffi.a");

    // Link fc_crypto_bridge and its transitive deps (fc, secp256k1, OpenSSL, …):
    // orchestrated mode links the archives the outer CMake build produced
    // (BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR plus a shell-style "-Ldir -llib …"
    // string in BEEKEEPER_FC_LINK_FLAGS); standalone mode drives CMake itself,
    // then prelinks everything into a single localized bundle.
    println!("cargo:rerun-if-env-changed=BEEKEEPER_FC_CRYPTO_BRIDGE_INCLUDE_DIR");
    println!("cargo:rerun-if-env-changed=BEEKEEPER_FC_LINK_FLAGS");

    if let Ok(dir) = env::var("BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR") {
        println!("cargo:rustc-link-search=native={}", out_dir.display());
        println!("cargo:rustc-link-lib=static=beekeeper_rust_ffi");
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
        prelink_native_bundle(&manifest_dir, &out_dir, &ffi_archive);
    }

    println!("cargo:rerun-if-changed=src/lib.rs");
    println!(
        "cargo:rerun-if-changed=include/beekeeper_rs/rust_wallet_storage.hpp"
    );
    println!(
        "cargo:rerun-if-changed=include/beekeeper_rs/beekeeper_holder.hpp"
    );
}

/// Builds the beekeeper `fc_crypto_bridge` target (and its transitive fc /
/// secp256k1 deps) via CMake, then prelinks everything — including static
/// Boost, OpenSSL, zlib and bz2 — into one relocatable object with only the
/// cxx bridge symbols exported: `ld -r --gc-sections` rooted at the bridge
/// symbols drops unreachable code, and every surviving internal symbol is
/// localized. This makes the crate's fc copy invisible to the rest of the
/// process, so it can never cross-bind with another fc build linked into the
/// same binary (e.g. the one inside the wax crate's native bundle).
fn prelink_native_bundle(manifest_dir: &Path, out_dir: &Path, ffi_archive: &Path) {
    // Minimal driver that builds only fc_crypto_bridge + its deps, avoiding the
    // beekeeper daemon's Boost requirements the CI image cannot satisfy.
    let driver_dir = manifest_dir.join("fc_bridge_cmake");
    println!("cargo:rerun-if-changed={}", driver_dir.join("CMakeLists.txt").display());

    let mut cfg = cmake::Config::new(&driver_dir);
    cfg.build_target("fc_crypto_bridge")
        .profile("Release")
        .define("CMAKE_POSITION_INDEPENDENT_CODE", "ON")
        // The native code ships prebuilt inside the published crate, so
        // optimize for size rather than speed (default Release flags are -O3).
        // -fno-gnu-unique for the same reason as on the bridge build above.
        .define("CMAKE_C_FLAGS_RELEASE", "-Os -DNDEBUG")
        .define("CMAKE_CXX_FLAGS_RELEASE", "-Os -DNDEBUG -fno-gnu-unique")
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

    // Whole-archive set: every member of the project archives participates in
    // the prelink. NOTE: weak references never extract archive members, so
    // weak (inline/header) definitions such as
    // fc::exception_factory::instance() would silently resolve to null at
    // final link without this; --gc-sections still prunes whatever the bridge
    // symbols cannot reach. beekeeper_core_minimal sources are compiled into
    // the FFI archive above, so its CMake archive stays out of the set.
    let mut whole_archives = vec![ffi_archive.to_path_buf()];
    for lib in ["fc_crypto_bridge", "fc"] {
        let archive = find_archive(&build_dir, lib).unwrap_or_else(|| {
            panic!("lib{lib}.a not found under {}", build_dir.display())
        });
        whole_archives.push(archive);
    }

    // On-demand set: vendored crypto plus static Boost, OpenSSL, zlib and
    // bz2 — folding these in is what removes the corresponding shared-library
    // requirement from the final link. libstdc++ deliberately stays dynamic:
    // a second static C++ runtime breaks exception handling and RTTI in
    // processes containing other C++ code.
    let mut grouped_archives = Vec::new();
    for lib in ["secp256k1", "secp256k1_precomputed", "equihash"] {
        if let Some(archive) = find_archive(&build_dir, lib) {
            grouped_archives.push(archive);
        }
    }
    for component in ["chrono", "context", "coroutine", "date_time", "filesystem", "system", "thread"] {
        grouped_archives.push(find_static_lib(&format!("libboost_{component}.a")));
    }
    for lib in ["libssl.a", "libcrypto.a", "libz.a", "libbz2.a"] {
        grouped_archives.push(find_static_lib(lib));
    }

    // The cxx bridge symbols are the entire surface reachable from Rust, so
    // they are the gc roots and the only symbols left exported.
    let nm_out = run(Command::new("nm").arg(ffi_archive));
    let mut roots: Vec<&str> = nm_out
        .lines()
        .filter_map(|line| {
            let mut cols = line.split_whitespace();
            let (_addr, kind, name) = (cols.next()?, cols.next()?, cols.next()?);
            (kind == "T" && name.contains("cxxbridge1$")).then_some(name)
        })
        .collect();
    roots.sort_unstable();
    roots.dedup();
    assert!(!roots.is_empty(), "no cxxbridge symbols found in {}", ffi_archive.display());

    let prelink_dir = out_dir.join("prelink");
    std::fs::create_dir_all(&prelink_dir).expect("create prelink dir");

    let roots_file = prelink_dir.join("gc_roots.txt");
    let exports_file = prelink_dir.join("exported_symbols.txt");
    let undefined_flags: String = roots.iter().map(|s| format!("--undefined={s}\n")).collect();
    std::fs::write(&roots_file, undefined_flags).expect("write gc roots");
    // Trailing newline: prelink_bundle.sh counts these entries with `wc -l`.
    let mut exports = roots.join("\n");
    exports.push('\n');
    std::fs::write(&exports_file, exports).expect("write exported symbols");

    let bundle = prelink_dir.join("beekeeper_native.o");
    let mut ld = Command::new("ld");
    ld.arg("-r")
        .arg("--gc-sections")
        // Dissolves COMDAT groups: with them preserved, the final linker
        // deduplicates identically-named groups across objects and discards
        // this bundle's copy, breaking its (localized) internal relocations.
        .arg("--force-group-allocation")
        .arg(format!("@{}", roots_file.display()))
        .arg("--whole-archive")
        .args(&whole_archives)
        .arg("--no-whole-archive")
        .arg("--start-group")
        .args(&grouped_archives)
        .arg("--end-group")
        .arg("-o")
        .arg(&bundle);
    run(&mut ld);

    // Strip debug info and unneeded local symbols before localizing — the
    // bundle ships inside the published crate, so size matters.
    let mut strip = Command::new("objcopy");
    strip.arg("--strip-debug").arg("--strip-unneeded").arg(&bundle);
    run(&mut strip);

    let mut objcopy = Command::new("objcopy");
    objcopy
        .arg(format!("--keep-global-symbols={}", exports_file.display()))
        .arg(&bundle);
    run(&mut objcopy);

    // Prebuilt system archives (e.g. Boost) contain STB_GNU_UNIQUE symbols
    // that objcopy cannot localize; renaming them per bundle prevents
    // duplicate-symbol clashes with other prelinked bundles (e.g. wax's) in
    // the same binary.
    let nm_bundle = run(Command::new("nm").arg(&bundle));
    let renames: String = nm_bundle
        .lines()
        .filter_map(|line| {
            let mut cols = line.split_whitespace();
            let (_addr, kind, name) = (cols.next()?, cols.next()?, cols.next()?);
            (kind == "u").then(|| format!("{name} beekeeper_native${name}\n"))
        })
        .collect();
    if !renames.is_empty() {
        let renames_file = prelink_dir.join("unique_renames.txt");
        std::fs::write(&renames_file, renames).expect("write unique renames");
        let mut objcopy = Command::new("objcopy");
        objcopy
            .arg(format!("--redefine-syms={}", renames_file.display()))
            .arg(&bundle);
        run(&mut objcopy);
    }

    let archive = prelink_dir.join("libbeekeeper_native.a");
    let _ = std::fs::remove_file(&archive);
    run(Command::new("ar").arg("rcs").arg(&archive).arg(&bundle));

    println!("cargo:rustc-link-search=native={}", prelink_dir.display());
    println!("cargo:rustc-link-lib=static=beekeeper_native");
    println!("cargo:rustc-link-lib=stdc++");
}

/// Locates a static library: BOOST_ROOT / OPENSSL_ROOT_DIR first, then the
/// compiler's default search path.
fn find_static_lib(name: &str) -> PathBuf {
    for var in ["BOOST_ROOT", "OPENSSL_ROOT_DIR"] {
        let Ok(root) = env::var(var) else { continue };
        for sub in ["lib", "lib64"] {
            let candidate = PathBuf::from(&root).join(sub).join(name);
            if candidate.is_file() {
                return candidate;
            }
        }
    }

    let cc = env::var("CC").unwrap_or_else(|_| "cc".into());
    let out = run(Command::new(cc).arg(format!("-print-file-name={name}")));
    let candidate = PathBuf::from(out.trim());
    if candidate.is_absolute() && candidate.is_file() {
        return candidate;
    }

    panic!("static library {name} not found; set BOOST_ROOT/OPENSSL_ROOT_DIR or install its -dev package");
}

/// Runs a command, panicking with its stderr on failure; returns its stdout.
fn run(cmd: &mut Command) -> String {
    let output = cmd.output().unwrap_or_else(|e| {
        panic!("failed to spawn {:?}: {e}", cmd.get_program())
    });
    assert!(
        output.status.success(),
        "{:?} failed:\n{}",
        cmd,
        String::from_utf8_lossy(&output.stderr)
    );

    String::from_utf8_lossy(&output.stdout).into_owned()
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
