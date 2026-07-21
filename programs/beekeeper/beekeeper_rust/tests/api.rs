//! Ports of the TS `README.md` examples (`beekeeper_wasm/README.md`), with
//! the values the README prints turned into assertions.
//!
//! The TS examples run sequentially against one shared storage root and
//! build on each other's state; Rust tests run in parallel, so every
//! persistent test here uses its own tempdir and recreates the state it
//! depends on.

use std::time::{Duration, SystemTime};

use beekeeper::prelude::*;
use tempfile::TempDir;

type Res = Result<(), BeekeeperError>;

const WIF1: &str = "5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT";
const WIF2: &str = "5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78";
/// Public counterpart of [`WIF2`] — the key the README examples print.
const PUBLIC_KEY2: &str =
    "STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa";

// "Importing and removing keys from a wallet"
#[test]
fn importing_and_removing_keys_from_a_wallet() -> Res {
    let beekeeper =
        BeekeeperApi::new(BeekeeperOptions::default().in_memory(true));

    let session = beekeeper.create_session()?;

    let mut wallet = session.create_wallet("wallet0", "password")?;

    let public_key1 = wallet.import_key(WIF1)?;
    let public_key2 = wallet.import_key(WIF2)?;

    wallet.remove_key(&public_key1)?;

    assert!(!wallet.has_matching_private_key(&public_key1)?);

    assert_eq!(public_key2, PUBLIC_KEY2);
    assert_eq!(wallet.get_public_keys()?, vec![public_key2]);

    // TS: session.close() — the Rust session closes itself on Drop.
    Ok(())
}

// "Listing all wallets"
#[test]
fn listing_all_wallets() -> Res {
    let beekeeper =
        BeekeeperApi::new(BeekeeperOptions::default().in_memory(true));

    let session = beekeeper.create_session()?;

    // "There should be a wallet created from the previous example" —
    // recreate it, since this test owns its own (in-memory) storage.
    session.create_wallet("wallet0", "password")?;

    let wallets = session.list_wallets()?;
    for wallet in &wallets {
        println!("Available wallet: {}", wallet.name); // Available wallet: wallet0
    }
    assert_eq!(
        wallets.iter().map(|w| w.name.as_str()).collect::<Vec<_>>(),
        vec!["wallet0"]
    );

    Ok(())
}

// "Retrieving session information"
#[test]
fn retrieving_session_information() -> Res {
    let beekeeper = BeekeeperApi::new(
        BeekeeperOptions::default()
            .unlock_timeout(10) // seconds
            .in_memory(true),
    );

    let session = beekeeper.create_session()?;

    let info = session.get_info();

    // Should be close to 10 seconds
    let locks_in = info
        .timeout_time
        .duration_since(SystemTime::now())
        .unwrap();

    println!(
        "All wallets will be automatically locked in {} ms if not used.",
        locks_in.as_millis()
    );
    assert!(locks_in <= Duration::from_secs(10));
    assert!(locks_in > Duration::from_secs(9));

    Ok(())
}

// "Use beekeeper in-memory"
#[test]
fn use_beekeeper_in_memory() -> Res {
    let beekeeper =
        BeekeeperApi::new(BeekeeperOptions::default().in_memory(true));

    // TS passes `true` as createWallet's third argument; Rust configures it
    // on the session, builder-style. (As in TS, it is optional when the
    // beekeeper itself is in-memory — all wallets are temporary then.)
    let session = beekeeper.create_session()?.is_temporary(true);

    let wallet = session.create_wallet("wallet1", "password")?;

    // console.log(wallet.isTemporary); // true
    assert!(wallet.is_temporary());

    // Clean up — dropping the session encrypts and removes all data from
    // memory, like TS session.close().
    Ok(())
}

// "Open wallet if exists"
#[test]
fn open_wallet_if_exists() -> Res {
    let tmp = TempDir::new().expect("tempdir");
    let storage_root = tmp.path().to_str().expect("utf8 path");

    let open_or_create =
        |session: &Session| -> Result<UnlockedWallet, BeekeeperError> {
            Ok(if session.has_wallet("wallet0")? {
                let locked_wallet = session.open_wallet("wallet0")?;

                locked_wallet.unlock("password")?
            } else {
                session.create_wallet("wallet0", "password")?.into_wallet()
            })
        };

    // First pass: nothing on disk yet — the create branch runs. Import the
    // key "from the previous example" so the second pass can find it.
    {
        let beekeeper = BeekeeperApi::new(BeekeeperOptions::new(storage_root));
        let session = beekeeper.create_session()?;
        let mut unlocked_wallet = open_or_create(&session)?;
        assert!(unlocked_wallet.get_public_keys()?.is_empty());
        unlocked_wallet.import_key(WIF2)?;
        drop(unlocked_wallet);
        drop(session);
        beekeeper.delete()?;
    }

    // Second pass: the wallet persisted — the open + unlock branch runs.
    let beekeeper = BeekeeperApi::new(BeekeeperOptions::new(storage_root));
    let session = beekeeper.create_session()?;
    let mut unlocked_wallet = open_or_create(&session)?;

    // Public key from the previous example:
    // [ 'STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa' ]
    assert_eq!(
        unlocked_wallet.get_public_keys()?,
        vec![PUBLIC_KEY2.to_string()]
    );

    Ok(())
}

// "Sign transaction digest"
#[test]
fn sign_transaction_digest() -> Res {
    let tmp = TempDir::new().expect("tempdir");
    let storage_root = tmp.path().to_str().expect("utf8 path");

    // Recreate the state from the earlier examples: wallet0 holding WIF2.
    {
        let beekeeper = BeekeeperApi::new(BeekeeperOptions::new(storage_root));
        let session = beekeeper.create_session()?;
        let mut wallet = session.create_wallet("wallet0", "password")?;
        wallet.import_key(WIF2)?;
        drop(wallet);
        drop(session);
        beekeeper.delete()?;
    }

    let beekeeper = BeekeeperApi::new(BeekeeperOptions::new(storage_root));
    let session = beekeeper.create_session()?;

    let mut unlocked_wallet =
        session.open_wallet("wallet0")?.unlock("password")?;

    // Example transaction digest (can be retrieved from the @hiveio/wax
    // library after creating a transaction)
    let sig_digest =
        "f1d3ff8443297732862df21dc4e57262a2b0b6f8c5f9f1d3ff8443297732862d";

    // This is the public key we imported into the wallet in previous example
    let my_public_key = PUBLIC_KEY2;

    let signature = unlocked_wallet.sign_digest(my_public_key, sig_digest)?;
    println!("Signature: {signature}");

    // The exact signature the TS README prints — fc signing is deterministic.
    assert_eq!(
        signature,
        "1f079d1b5aa5791cc0ea676978e4cf2a5c40ae30c893c9daa5ec01366b4b5a7ef92b1e22df42e294fc8c797f948b966eb21126a229bb992da4142d91212d974ee3"
    );

    Ok(())
}

// "Encrypt and decrypt message"
#[test]
fn encrypt_and_decrypt_message() -> Res {
    let beekeeper =
        BeekeeperApi::new(BeekeeperOptions::default().in_memory(true));

    let session = beekeeper.create_session()?;

    let mut wallet = session.create_wallet("wallet2", "password")?;

    let public_key1 = wallet.import_key(WIF1)?;
    let public_key2 = wallet.import_key(WIF2)?;

    let message = "Hello, Hive!";

    // TS: wallet.encryptData(message, publicKey1, publicKey2). Nonce 0 asks
    // the C++ side to generate a random one, so (unlike the README's printed
    // ciphertext) the output is not reproducible — assert the round-trip.
    let encrypted =
        wallet.encrypt_data(&public_key1, public_key2.as_str(), message, 0)?;
    println!("Encrypted message: {encrypted}");
    assert!(!encrypted.is_empty());

    let decrypted =
        wallet.decrypt_data(&public_key1, public_key2.as_str(), &encrypted)?;

    // console.log('Decrypted message:', decrypted); // 'Hello, Hive!'
    assert_eq!(decrypted, message);

    Ok(())
}
