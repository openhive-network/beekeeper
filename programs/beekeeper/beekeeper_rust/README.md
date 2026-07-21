# hiveio-beekeeper

Rust bindings for [Beekeeper](https://gitlab.syncad.com/hive/beekeeper), the
Hive wallet component: encrypted key storage, wallet sessions with auto-lock,
key import/removal, digest signing and memo encryption/decryption.

## Usage

```rust
use beekeeper::prelude::*;

let bk = BeekeeperApi::new(
    BeekeeperOptions::new("./storage_root").unlock_timeout(900),
);
let session = bk.create_session().unwrap();
let mut created = session.create_wallet("my-wallet", "password").unwrap();
let pubkey = created.import_key("5J...wif...").unwrap();
let sig = created.sign_digest(&pubkey, "deadbeef...").unwrap();
// Dropping `session` locks its wallets and closes the session.
```

## Platform support

Linux x86_64. The bundle is built per target triple; other platforms need a
regenerated bundle and are not published yet.

## License

MIT — see the [beekeeper repository](https://gitlab.syncad.com/hive/beekeeper).
