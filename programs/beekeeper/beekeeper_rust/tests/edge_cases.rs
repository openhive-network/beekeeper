//! Mirrors `beekeeper_wasm/__tests__/detailed/edge_cases.ts`. Focuses on
//! error paths and lock/unlock state-machine behavior reachable from the
//! Rust public API. TS-only paths (auto-lock getter, stale references) are
//! skipped — those are JS-layer semantics.

mod common;

use common::{KEYS, new_in_memory, new_persistent};

// "Should be able to open and unlock a previously created wallet via openWallet"
#[test]
fn create_close_then_open_and_unlock() {
    let bk = new_persistent();
    let session = bk.api.create_session().unwrap();
    // Create + close
    {
        let mut wallet = session.create_wallet("w0", "otherpass").unwrap();
        wallet.import_key(KEYS[0].0).unwrap();
        wallet.close().unwrap();
    }
    // Re-open + unlock
    let mut unlocked = session
        .open_wallet("w0")
        .unwrap()
        .unlock("otherpass")
        .unwrap();
    let keys = unlocked.get_public_keys().unwrap();
    assert_eq!(keys, vec![KEYS[0].1.to_string()]);
}

// "Should be able to lock all wallets via session.lockAll"
#[test]
fn lock_all_locks_session_wallets() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();

    let mut w0 = session.create_wallet("w0", "pass0").unwrap().wallet;
    w0.import_key(KEYS[0].0).unwrap();
    let _locked0 = w0.lock().unwrap();

    let mut w1 = session.create_wallet("w1", "pass1").unwrap().wallet;
    w1.import_key(KEYS[1].0).unwrap();
    let _locked1 = w1.lock().unwrap();

    // Now re-open one, leave unlocked, lock_all and verify
    let _ = session.open_wallet("w0").unwrap().unlock("pass0").unwrap();
    let locked_listing = session.lock_all().unwrap();
    assert!(locked_listing.iter().all(|w| !w.unlocked));
    assert!(locked_listing.iter().any(|w| w.name == "w0"));
}

// "Should be able to lock and unlock a wallet via the high-level API"
#[test]
fn lock_then_unlock_recovers_keys() {
    let bk = new_persistent();
    let session = bk.api.create_session().unwrap();
    let mut wallet = session.create_wallet("w0", "mypassword").unwrap().wallet;
    wallet.import_key(KEYS[0].0).unwrap();
    let locked = wallet.lock().unwrap();
    let mut unlocked = locked.unlock("mypassword").unwrap();
    let keys = unlocked.get_public_keys().unwrap();
    assert_eq!(keys, vec![KEYS[0].1.to_string()]);
}

// "Should throw ... when unlocking with wrong password"
#[test]
fn unlock_with_wrong_password_errors() {
    let bk = new_persistent();
    let session = bk.api.create_session().unwrap();
    let wallet = session
        .create_wallet("w0", "correct_password")
        .unwrap()
        .wallet;
    let locked = wallet.lock().unwrap();
    let err = locked.unlock("wrong_password");
    assert!(err.is_err(), "expected unlock with wrong password to fail");
}

// "Should throw as the wallet is not found" (importKey to nonexistent wallet)
#[test]
fn open_nonexistent_wallet_errors() {
    let bk = new_persistent();
    let session = bk.api.create_session().unwrap();
    let res = session.open_wallet("never_created");
    assert!(res.is_err());
}

// "Should throw as the wallet with the same name already exists"
#[test]
fn duplicate_wallet_create_errors() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    session.create_wallet("w0", "pw").unwrap();
    let res = session.create_wallet("w0", "pw");
    assert!(res.is_err(), "creating wallet twice should error");
}

// "Should throw when querying keys without a wallet"
#[test]
fn get_keys_on_nonexistent_wallet_errors() {
    let bk = new_persistent();
    let session = bk.api.create_session().unwrap();
    // open_wallet for a name that doesn't exist on disk should fail
    let res = session.open_wallet("nonexistent");
    assert!(res.is_err());
}

// "Should throw as the key is invalid"
#[test]
fn invalid_wif_errors() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let mut wallet = session.create_wallet("w0", "pw").unwrap();
    let res = wallet.import_key("not-a-real-wif-key");
    assert!(res.is_err());
}

// "Should throw as the key is not in the wallet" (removeKey on missing key)
#[test]
fn remove_missing_key_errors() {
    let bk = new_in_memory();
    let session = bk.api.create_session().unwrap();
    let mut wallet = session.create_wallet("w0", "pw").unwrap();
    let res = wallet.remove_key(KEYS[7].1);
    assert!(res.is_err());
}
