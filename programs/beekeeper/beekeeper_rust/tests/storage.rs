//! Rust-equivalent of `beekeeper_wasm/__tests__/detailed/storage.ts`.
//!
//! The TS file tests IndexedDB-backed storage across browser contexts —
//! that's a browser-only concept with no native analog. The functional
//! equivalents on the Rust file-system backend are:
//!
//! - persistence across separate `BeekeeperApi` instances over the same dir
//! - isolation between separate dirs
//! - file persistence with and without explicit `delete()`

use beekeeper::prelude::*;
use tempfile::TempDir;

mod common;
use common::KEYS;

// Equivalent of "Should be able to persist data to IndexedDB and read it back".
// We create a wallet in one instance, then open a fresh instance over the
// same storage_root and find the wallet listed.
#[test]
fn persists_across_api_instances() {
    let tmp = TempDir::new().unwrap();
    let root = tmp.path().to_str().unwrap().to_string();

    {
        let bk = BeekeeperApi::new(BeekeeperOptions::new(&root));
        let session = bk.create_session().unwrap();
        session.create_wallet("test_wallet", "pass123").unwrap();
        drop(session);
        bk.delete().unwrap();
    }

    let bk = BeekeeperApi::new(BeekeeperOptions::new(&root));
    let names: Vec<String> = bk.list_created_wallets().into_iter().collect();
    assert_eq!(names, vec!["test_wallet"]);
}

// Equivalent of "Should not contain any data from the previous test in a new context".
// A fresh storage_root has no wallets.
#[test]
fn fresh_storage_root_is_empty() {
    let tmp = TempDir::new().unwrap();
    let bk =
        BeekeeperApi::new(BeekeeperOptions::new(tmp.path().to_str().unwrap()));
    assert!(bk.list_created_wallets().is_empty());
}

// Equivalent of "Should be able to list the previously imported key from another
// page with the same browser context". We create + import keys in instance A,
// then in a separate instance B (same dir) open the wallet, unlock, list keys.
#[test]
fn keys_persist_across_api_instances() {
    let tmp = TempDir::new().unwrap();
    let root = tmp.path().to_str().unwrap().to_string();

    // Instance A — create wallet and import a key, then delete the instance.
    {
        let bk = BeekeeperApi::new(BeekeeperOptions::new(&root));
        let session = bk.create_session().unwrap();
        let mut wallet = session.create_wallet("w0", "badf00d").unwrap().wallet;
        wallet.import_key(KEYS[0].0).unwrap();
        drop(wallet);
        drop(session);
        bk.delete().unwrap();
    }

    // Instance B — same dir, open the wallet, unlock with the same password,
    // verify the imported key is still there.
    {
        let bk = BeekeeperApi::new(BeekeeperOptions::new(&root));
        let session = bk.create_session().unwrap();
        let mut unlocked = session
            .open_wallet("w0")
            .unwrap()
            .unlock("badf00d")
            .unwrap();
        let keys = unlocked.get_public_keys().unwrap();
        assert_eq!(keys, vec![KEYS[0].1.to_string()]);
    }
}

// Equivalent of "Should not be able to access previously created wallet from
// other context" — two distinct storage_roots are isolated.
#[test]
fn separate_storage_roots_are_isolated() {
    let tmp_a = TempDir::new().unwrap();
    let tmp_b = TempDir::new().unwrap();

    {
        let bk = BeekeeperApi::new(BeekeeperOptions::new(
            tmp_a.path().to_str().unwrap(),
        ));
        let session = bk.create_session().unwrap();
        session.create_wallet("only_in_a", "pw").unwrap();
        drop(session);
        bk.delete().unwrap();
    }

    let bk_b = BeekeeperApi::new(BeekeeperOptions::new(
        tmp_b.path().to_str().unwrap(),
    ));
    assert!(bk_b.list_created_wallets().is_empty());
}

// Equivalent of "without explicitly closing the instance of beekeeper" — when
// the API value is dropped without calling `delete()`, on-disk wallets still
// persist because `save_fn` writes synchronously.
#[test]
fn persistence_without_explicit_delete() {
    let tmp = TempDir::new().unwrap();
    let root = tmp.path().to_str().unwrap().to_string();

    {
        let bk = BeekeeperApi::new(BeekeeperOptions::new(&root));
        let session = bk.create_session().unwrap();
        session.create_wallet("w0", "pw").unwrap();
        // No bk.delete() — let everything drop.
    }

    let bk = BeekeeperApi::new(BeekeeperOptions::new(&root));
    let names = bk.list_created_wallets();
    assert!(names.contains(&"w0".to_string()));
}
