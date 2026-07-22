# hiveio-beekeeper

Rust bindings for [Beekeeper](https://gitlab.syncad.com/hive/beekeeper), the
Hive wallet component: encrypted key storage, wallet sessions with auto-lock,
key import/removal, digest signing and memo encryption/decryption.

## Installation

```bash
cargo add hiveio-beekeeper
```

The library target is named `beekeeper`, so imports read
`use beekeeper::prelude::*;`.

## Example usage

Runnable versions of all examples below live in [examples.rs](examples.rs);
run them in order with `cargo run --example readme`. The persistent examples
share `./storage_root` under the current directory and build on each other's
state.

### Importing and removing keys from a wallet

Note: wallet names must be unique within one beekeeper instance — creating
a wallet that already exists there is an error.

Remember to always use strong passwords for wallets! If you pass `None` as
the password when creating a wallet, a strong random password is generated
for you and returned in `CreatedWallet::password`.

```rust
use beekeeper::prelude::*;

fn main() -> Result<(), BeekeeperError> {
    let beekeeper = BeekeeperApi::new(BeekeeperOptions::new("./storage_root"));

    let session = beekeeper.create_session()?;

    // Errors if the wallet already exists in this beekeeper instance.
    let mut wallet = session.create_wallet("wallet0", "password")?.wallet;

    // The returned value is the public key of the imported private key.
    // Keys can be generated with the wax library or any other library
    // that can generate Hive keys.
    let public_key1 = wallet
        .import_key("5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT")?;
    let public_key2 = wallet
        .import_key("5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78")?;

    wallet.remove_key(&public_key1)?;

    println!("{}", wallet.has_matching_private_key(&public_key1)?); // false

    // Only `public_key2` remains in the wallet.
    assert_eq!(wallet.get_public_keys()?, vec![public_key2]);

    // Dropping the session locks and closes all wallets it opened.
    Ok(())
}
```

### Listing all wallets

```rust
use beekeeper::prelude::*;

fn main() -> Result<(), BeekeeperError> {
    let beekeeper = BeekeeperApi::new(BeekeeperOptions::new("./storage_root"));

    let session = beekeeper.create_session()?;

    // There should be a wallet created from the previous example.
    for wallet in session.list_created_wallets() {
        println!("Available wallet: {}", wallet.name); // Available wallet: wallet0
    }

    Ok(())
}
```

### Retrieving session information

```rust
use std::time::SystemTime;

use beekeeper::prelude::*;

fn main() -> Result<(), BeekeeperError> {
    let beekeeper = BeekeeperApi::new(
        BeekeeperOptions::default()
            .unlock_timeout(10) // seconds
            .in_memory(true),
    );

    let session = beekeeper.create_session()?;

    let info = session.get_info();

    // Should be close to 10 seconds.
    let locks_in = info.timeout_time.duration_since(SystemTime::now()).unwrap();

    println!(
        "All wallets will be automatically locked in {} ms if not used.",
        locks_in.as_millis()
    );

    Ok(())
}
```

If you still want to use the wallet after the timeout, `lock()` the handle
and `unlock()` it again.

**Every time you use the wallet, the timeout is reset.**

### Use beekeeper in-memory

```rust
use beekeeper::prelude::*;

fn main() -> Result<(), BeekeeperError> {
    // No data is persisted to disk.
    let beekeeper =
        BeekeeperApi::new(BeekeeperOptions::default().in_memory(true));

    // `is_temporary(true)` marks wallets created through this session as
    // in-memory-only. It is optional when the beekeeper itself is
    // in-memory — every wallet is temporary then.
    let session = beekeeper.create_session()?.is_temporary(true);

    let wallet = session.create_wallet("wallet1", "password")?.wallet;

    println!("{}", wallet.is_temporary()); // true

    // Dropping the session encrypts and removes all data from memory.
    Ok(())
}
```

### Open wallet if exists

```rust
use beekeeper::prelude::*;

fn main() -> Result<(), BeekeeperError> {
    let beekeeper = BeekeeperApi::new(BeekeeperOptions::new("./storage_root"));

    let session = beekeeper.create_session()?;

    let mut unlocked_wallet = if session.has_wallet("wallet0")? {
        session.open_wallet("wallet0")?.unlock("password")?
    } else {
        session.create_wallet("wallet0", "password")?.wallet
    };

    // Public key from the first example:
    // ["STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"]
    println!("{:?}", unlocked_wallet.get_public_keys()?);

    Ok(())
}
```

### Sign transaction digest

```rust
use beekeeper::prelude::*;

fn main() -> Result<(), BeekeeperError> {
    let beekeeper = BeekeeperApi::new(BeekeeperOptions::new("./storage_root"));

    let session = beekeeper.create_session()?;

    let mut unlocked_wallet =
        session.open_wallet("wallet0")?.unlock("password")?;

    // Example transaction digest (can be retrieved e.g. from the wax
    // library after creating a transaction).
    let sig_digest =
        "f1d3ff8443297732862df21dc4e57262a2b0b6f8c5f9f1d3ff8443297732862d";

    // The public key imported into the wallet in the first example.
    let my_public_key = "STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa";

    // Sign the digest with the private key matching `my_public_key`.
    let signature = unlocked_wallet.sign_digest(my_public_key, sig_digest)?;

    // Signature: 1f079d1b5aa5791cc0ea676978e4cf2a5c40ae30c893c9daa5ec0136
    //            6b4b5a7ef92b1e22df42e294fc8c797f948b966eb21126a229bb992d
    //            a4142d91212d974ee3
    println!("Signature: {signature}");

    Ok(())
}
```

### Encrypt and decrypt message

```rust
use beekeeper::prelude::*;

fn main() -> Result<(), BeekeeperError> {
    let beekeeper = BeekeeperApi::new(BeekeeperOptions::new("./storage_root"));

    let session = beekeeper.create_session()?;

    let mut wallet = session.create_wallet("wallet2", "password")?.wallet;

    let public_key1 = wallet
        .import_key("5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT")?;
    let public_key2 = wallet
        .import_key("5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78")?;

    let message = "Hello, Hive!";

    // Nonce 0 asks the C++ side to generate a random nonce, so the
    // ciphertext differs between runs.
    let encrypted =
        wallet.encrypt_data(&public_key1, public_key2.as_str(), message, 0)?;
    println!("Encrypted message: {encrypted}");

    let decrypted =
        wallet.decrypt_data(&public_key1, public_key2.as_str(), &encrypted)?;
    println!("Decrypted message: {decrypted}"); // Hello, Hive!

    Ok(())
}
```

## Platform support

Linux x86_64. The bundle is built per target triple; other platforms need a
regenerated bundle and are not published yet.

## License

MIT — see the [beekeeper repository](https://gitlab.syncad.com/hive/beekeeper).
