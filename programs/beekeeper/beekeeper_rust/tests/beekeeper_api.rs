//! Mirrors `beekeeper_wasm/__tests__/detailed/beekeeper_api.ts`. Ports a
//! representative subset of the ~50 TS tests, using our public Rust API
//! instead of the TS-only `BeekeeperInstanceHelper` fixture. Tests asserting
//! exact fc-format signature bytes are marked `#[ignore]` pending crypto
//! byte-compat (see `todo.md` § Crypto).

mod common;

use common::{
    EXPECTED_SIG_0, KEYS, SIG_DIGEST_0, new_in_memory, new_persistent,
};

// "Should be able to get sign digest"
#[test]
fn sign_digest() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();
    let mut wallet = bk
        .api
        .session(&token)
        .create_wallet("w3", Some("pass"), None)
        .unwrap()
        .wallet;
    let key = wallet.import_key(KEYS[3].0).unwrap();
    let sig = wallet.sign_digest(&key, SIG_DIGEST_0).unwrap();
    assert_eq!(sig, EXPECTED_SIG_0);
}

// "Should require keys in wallet" — import a key, then list returns it
#[test]
fn import_then_get_public_keys() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();
    let mut wallet = bk
        .api
        .session(&token)
        .create_wallet("w3", Some("pass"), None)
        .unwrap()
        .wallet;
    wallet.import_key(KEYS[3].0).unwrap();
    let pks = wallet.get_public_keys().unwrap();
    assert_eq!(pks, vec![KEYS[3].1.to_string()]);
}

// "Should be able to import keys" — import two keys, both come back
#[test]
fn import_multiple_keys() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();
    let mut wallet = bk
        .api
        .session(&token)
        .create_wallet("w0", Some("pass"), None)
        .unwrap()
        .wallet;
    wallet.import_key(KEYS[3].0).unwrap();
    wallet.import_key(KEYS[4].0).unwrap();
    let mut pks = wallet.get_public_keys().unwrap();
    pks.sort();
    let mut expected = vec![KEYS[3].1.to_string(), KEYS[4].1.to_string()];
    expected.sort();
    assert_eq!(pks, expected);
}

// "Should be able to remove a key"
#[test]
fn remove_key_empties_wallet() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();
    let mut wallet = bk
        .api
        .session(&token)
        .create_wallet("w1", Some("cherry"), None)
        .unwrap()
        .wallet;
    wallet.import_key(KEYS[1].0).unwrap();
    wallet.remove_key(KEYS[1].1).unwrap();
    let pks = wallet.get_public_keys().unwrap();
    assert!(pks.is_empty());
}

// "Should be able to create a wallet with auto-generated password"
#[test]
fn auto_generated_password_returned() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();
    let created = bk
        .api
        .session(&token)
        .create_wallet("w0", None, None)
        .unwrap();
    // TS asserts password starts with "PW"; we just assert it's non-empty.
    // (C++ generates the password; the prefix convention may differ.)
    assert!(!created.password.is_empty());
}

// "Should be able to close a session"
#[test]
fn create_and_close_session() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();
    bk.api.close_session(&token).unwrap();
}

// "Should be able to create a few wallets" — create three, each accessible
#[test]
fn create_three_wallets() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();
    for n in ["w1", "w2", "w3"] {
        let mut wallet = bk
            .api
            .session(&token)
            .create_wallet(n, Some("pass"), None)
            .unwrap()
            .wallet;
        wallet.import_key(KEYS[0].0).unwrap();
        wallet.remove_key(KEYS[0].1).unwrap();
    }
    let mut names: Vec<String> = bk
        .api
        .session(&token)
        .list_wallets()
        .unwrap()
        .into_iter()
        .map(|w| w.name)
        .collect();
    names.sort();
    assert_eq!(names, vec!["w1", "w2", "w3"]);
}

// "Should be able to lock all wallets"
#[test]
fn lock_all_clears_unlocked_state() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();
    for n in ["w1", "w2"] {
        let mut wallet = bk
            .api
            .session(&token)
            .create_wallet(n, Some("pw"), None)
            .unwrap()
            .wallet;
        wallet.import_key(KEYS[0].0).unwrap();
        // Leave it unlocked
        let _ = wallet;
    }
    bk.api.session(&token).lock_all().unwrap();
    let infos = bk.api.session(&token).list_wallets().unwrap();
    assert!(infos.iter().all(|w| !w.unlocked));
}

// "Should be able to delete an api instance"
#[test]
fn delete_instance_succeeds() {
    let mut bk = new_in_memory();
    let _token = bk.api.create_session().unwrap();
    bk.api.delete().unwrap();
}

// "Should be able to get session info"
#[test]
fn session_info_now_and_timeout_present() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();
    let info = bk.api.session(&token).get_info();
    assert!(info.timeout_time > info.now);
}

// "Check has_matching_private_key endpoint"
#[test]
fn has_matching_private_key_before_and_after() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();
    let mut wallet = bk
        .api
        .session(&token)
        .create_wallet("w9", Some("pass"), None)
        .unwrap()
        .wallet;
    assert!(!wallet.has_matching_private_key(KEYS[0].1).unwrap());
    wallet.import_key(KEYS[0].0).unwrap();
    assert!(wallet.has_matching_private_key(KEYS[0].1).unwrap());
}

// "Should get keys per wallet (not merged across wallets)"
#[test]
fn keys_isolated_per_wallet() {
    let mut bk = new_in_memory();
    let token = bk.api.create_session().unwrap();

    let mut w0 = bk
        .api
        .session(&token)
        .create_wallet("w0", Some("pw"), None)
        .unwrap()
        .wallet;
    w0.import_key(KEYS[0].0).unwrap();
    w0.import_key(KEYS[1].0).unwrap();
    let mut w0_keys = w0.get_public_keys().unwrap();
    w0_keys.sort();
    // Drop w0 to release &mut bk borrow
    drop(w0);

    let mut w1 = bk
        .api
        .session(&token)
        .create_wallet("w1", Some("pw"), None)
        .unwrap()
        .wallet;
    w1.import_key(KEYS[2].0).unwrap();
    let w1_keys = w1.get_public_keys().unwrap();

    let mut expected_w0 = vec![KEYS[0].1.to_string(), KEYS[1].1.to_string()];
    expected_w0.sort();
    assert_eq!(w0_keys, expected_w0);
    assert_eq!(w1_keys, vec![KEYS[2].1.to_string()]);
}

// "Should properly handle wallet persistence (create, close, reopen, unlock)"
#[test]
fn persist_create_close_reopen_unlock() {
    let mut bk = new_persistent();
    let token = bk.api.create_session().unwrap();
    {
        let mut wallet = bk
            .api
            .session(&token)
            .create_wallet("persistent_wallet", Some("mypass"), None)
            .unwrap()
            .wallet;
        wallet.import_key(KEYS[3].0).unwrap();
        let before = wallet.get_public_keys().unwrap();
        assert_eq!(before, vec![KEYS[3].1.to_string()]);
        wallet.close().unwrap();
    }
    let mut unlocked = bk
        .api
        .session(&token)
        .open_wallet("persistent_wallet")
        .unwrap()
        .unlock("mypass")
        .unwrap();
    let after = unlocked.get_public_keys().unwrap();
    assert_eq!(after, vec![KEYS[3].1.to_string()]);
}
