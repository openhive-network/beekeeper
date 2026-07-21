//! Mirrors `beekeeper_wasm/__tests__/detailed/factory.ts`. Each test below
//! references the TS test name it ports.
//!
//! Tests that exercise TS auto-lock-on-getter / stale-reference semantics are
//! skipped — those are TS-layer behaviors enforced via JS getters, not C++.

mod common;

use beekeeper::prelude::*;
use common::{
    EXPECTED_SIG_0, KEYS, PUBKEY_KEY3, SIG_DIGEST_0, new_in_memory,
    new_persistent,
};

// "Should be able to retrieve package version"
#[test]
fn version_is_string() {
    let bk = new_in_memory();
    assert!(!bk.api.version().is_empty());
}

// "Should be able to init the beekeeper factory"
#[test]
fn init_create_session_delete() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    drop(session);
    bk.api.delete().unwrap();
}

// "Should be able to get_info based on the created session"
#[test]
fn get_info_returns_now_and_timeout() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let info = session.get_info();
    assert!(info.timeout_time > info.now);
}

// "Should be able to create multiple sessions with wallets and delete the beekeeper instance"
#[test]
fn multiple_sessions_and_wallets() {
    let bk = new_in_memory();
    let s1 = bk.api.create_session().unwrap();
    let s2 = bk.api.create_session().unwrap();
    s1.create_wallet("w0", None).unwrap();
    s2.create_wallet("w1", None).unwrap();
    drop(s1);
    drop(s2);
    bk.api.delete().unwrap();
}

// "Should be able to create a wallet and import and remove keys"
#[test]
fn import_then_remove_key() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let mut unlocked =
        session.create_wallet("w0", "mypassword").unwrap().wallet;
    unlocked.import_key(KEYS[0].0).unwrap();
    unlocked.import_key(KEYS[1].0).unwrap();
    unlocked.remove_key(KEYS[1].1).unwrap();

    let pks = unlocked.get_public_keys().unwrap();
    assert_eq!(pks, vec![KEYS[0].1.to_string()]);
}

// "Should be able to create a wallet, import keys and check if matching key exists"
#[test]
fn has_matching_private_key_after_import() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let mut unlocked =
        session.create_wallet("w0", "mypassword").unwrap().wallet;
    unlocked.import_key(KEYS[0].0).unwrap();
    assert!(unlocked.has_matching_private_key(KEYS[0].1).unwrap());
}

// "Should be able to do a single inMemory sign"
#[test]
fn in_memory_sign_roundtrip() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let mut wallet = session.create_wallet("tmp", "mypassword").unwrap();
    let pubkey = wallet.import_key(KEYS[3].0).unwrap();
    let sig = wallet.sign_digest(&pubkey, SIG_DIGEST_0).unwrap();
    assert_eq!(sig, EXPECTED_SIG_0);
    drop(wallet);
    drop(session);
    bk.api.delete().unwrap();
}

// "Should wallet be a temporary wallet when inMemory is true"
#[test]
fn in_memory_creates_temporary_default() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let created = session.create_wallet("w0", "pw").unwrap();
    assert!(created.is_temporary());
    let info = session.list_wallets().unwrap();
    assert!(info.iter().any(|w| w.name == "w0" && w.is_temporary));
}

// "Should wallet not be temporary by default" (persistent API)
#[test]
fn persistent_default_not_temporary() {
    let bk = new_persistent();
    let session = bk.api.create_session().unwrap();
    let created = session.create_wallet("w0", "pw").unwrap();
    assert!(!created.is_temporary());
    let info = session.list_wallets().unwrap();
    assert!(info.iter().any(|w| w.name == "w0" && !w.is_temporary));
}

// "Should be able to create an explicitly temporary wallet"
#[test]
fn explicit_temporary_wallet() {
    let bk = new_persistent();
    let session = bk.api.create_session().unwrap().is_temporary(true);
    session.create_wallet("w0", "pw").unwrap();
    let info = session.list_wallets().unwrap();
    assert!(info.iter().any(|w| w.name == "w0" && w.is_temporary));
}

// "Should have a wallet via hasWallet after creation"
#[test]
fn has_wallet_after_create() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    session.create_wallet("w0", "pw").unwrap();
    assert!(session.has_wallet("w0").unwrap());
}

// "Should not have a non-existent wallet via hasWallet"
#[test]
fn has_wallet_missing_is_false() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    assert!(!session.has_wallet("nope").unwrap());
}

// "Should be able to create multiple wallets and access them using listWallets references"
#[test]
fn list_wallets_returns_created() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    for n in ["w0", "w1", "w2"] {
        session.create_wallet(n, "pw").unwrap();
    }
    let mut names: Vec<String> = session
        .list_wallets()
        .unwrap()
        .into_iter()
        .map(|w| w.name)
        .collect();
    names.sort();
    assert_eq!(names, vec!["w0", "w1", "w2"]);
}

// "Should be able to encrypt and decrypt data (round-trip)"
#[test]
fn encrypt_decrypt_roundtrip() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let mut wallet = session.create_wallet("w0", "pw").unwrap();
    let key = wallet.import_key(KEYS[7].0).unwrap();
    let ct = wallet
        .encrypt_data(&key, None, "hello beekeeper", 12345)
        .unwrap();
    let pt = wallet.decrypt_data(&key, None, &ct).unwrap();
    assert_eq!(pt, "hello beekeeper");
    assert!(!ct.is_empty());
}

// "Should be able to encrypt for another key and decrypt with that key"
#[test]
fn encrypt_cross_key_roundtrip() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let mut wallet = session.create_wallet("w0", "pw").unwrap();
    let key1 = wallet.import_key(KEYS[7].0).unwrap();
    let key2 = wallet.import_key(KEYS[3].0).unwrap();
    let ct = wallet
        .encrypt_data(&key1, key2.as_str(), "secret", 67890)
        .unwrap();
    let pt = wallet.decrypt_data(&key1, key2.as_str(), &ct).unwrap();
    assert_eq!(pt, "secret");
}

// "Should produce deterministic encryption with explicit nonce"
#[test]
fn encrypt_deterministic_with_nonce() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let mut wallet = session.create_wallet("w0", "pw").unwrap();
    let key = wallet.import_key(KEYS[7].0).unwrap();
    let ct1 = wallet.encrypt_data(&key, None, "det", 99999).unwrap();
    let ct2 = wallet.encrypt_data(&key, None, "det", 99999).unwrap();
    assert_eq!(ct1, ct2);
}

// "Should be able to sign digest"
#[test]
fn sign_digest_matches_expected() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let mut wallet = session.create_wallet("w0", "pw").unwrap();
    let pk = wallet.import_key(KEYS[3].0).unwrap();
    assert_eq!(pk, PUBKEY_KEY3);
    let sig = wallet.sign_digest(&pk, SIG_DIGEST_0).unwrap();
    assert_eq!(sig, EXPECTED_SIG_0);
}

// "Should be able to create a beekeeper instance with custom unlockTimeout"
#[test]
fn custom_unlock_timeout_get_info() {
    let bk = BeekeeperApi::new(
        BeekeeperOptions::new("ignored")
            .in_memory(true)
            .unlock_timeout(5),
    );
    let session = bk.create_session().unwrap();
    let info = session.get_info();
    assert!(info.timeout_time > info.now);
}

// "Should persist wallets across re-initialization"
#[test]
fn wallets_persist_across_reinit() {
    let tmp = tempfile::TempDir::new().expect("tempdir");
    let root = tmp.path().to_str().unwrap().to_string();

    {
        let bk = BeekeeperApi::new(BeekeeperOptions::new(&root));
        let session = bk.create_session().unwrap();
        let mut wallet =
            session.create_wallet("w0", "mypassword").unwrap().wallet;
        wallet.import_key(KEYS[0].0).unwrap();
        drop(wallet);
        drop(session);
        bk.delete().unwrap();
    }

    {
        let bk = BeekeeperApi::new(BeekeeperOptions::new(&root));
        let session = bk.create_session().unwrap();
        let names: Vec<String> = session
            .list_created_wallets()
            .into_iter()
            .map(|w| w.name)
            .collect();
        assert!(names.contains(&"w0".to_string()));

        let mut unlocked = session
            .open_wallet("w0")
            .unwrap()
            .unlock("mypassword")
            .unwrap();
        let keys = unlocked.get_public_keys().unwrap();
        assert_eq!(keys, vec![KEYS[0].1.to_string()]);
        drop(unlocked);
        drop(session);
        bk.delete().unwrap();
    }
}
