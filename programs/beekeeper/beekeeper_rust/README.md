# hiveio-beekeeper

Rust bindings for [Beekeeper](https://gitlab.syncad.com/hive/beekeeper), the
Hive wallet component: encrypted key storage, wallet sessions with auto-lock,
key import/removal, digest signing and memo encryption/decryption.

## Usage

```rust
use beekeeper::{BeekeeperApi, BeekeeperOptions};

let mut bk = BeekeeperApi::new(
    BeekeeperOptions::new("./storage_root").unlock_timeout(900),
);
let token = bk.create_session().unwrap();
let created = bk.session(&token)
    .create_wallet("my-wallet", Some("password"), None)
    .unwrap();
let mut wallet = created.wallet;
let pubkey = wallet.import_key("5J...wif...").unwrap();
let sig = wallet.sign_digest(&pubkey, "deadbeef...").unwrap();
```

## Platform support

Linux x86_64. The bundle is built per target triple; other platforms need a
regenerated bundle and are not published yet.

## License

MIT — see the [beekeeper repository](https://gitlab.syncad.com/hive/beekeeper).
