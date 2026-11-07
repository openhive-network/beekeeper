# Beekeeper

Standalone wallet daemon with HTTP/WebSocket API for the Hive blockchain.

Beekeeper provides secure key management and transaction signing without requiring a full blockchain node.

## Packages

| Surface                        | Package                                                                | Documentation                                                                              |
|--------------------------------|------------------------------------------------------------------------|--------------------------------------------------------------------------------------------|
| Native daemon                  | `beekeeper` (built from this repository)                               | [Building](#building) and [Usage](#usage) below                                            |
| Python                         | [`hiveio-beekeepy`](https://pypi.org/project/hiveio-beekeepy/)         | [python/README.md](python/README.md)                                                       |
| TypeScript / JavaScript (WASM) | [`@hiveio/beekeeper`](https://www.npmjs.com/package/@hiveio/beekeeper) | [programs/beekeeper/beekeeper_wasm/README.md](programs/beekeeper/beekeeper_wasm/README.md) |

Python (3.12+, Linux - the package embeds the native daemon):

```bash
pip install hiveio-beekeepy
```

Development builds of `hiveio-beekeepy`, and the dependencies published only there, are in the Hive GitLab package registry: add `--extra-index-url https://gitlab.syncad.com/api/v4/groups/136/-/packages/pypi/simple`.

TypeScript / JavaScript (Node.js 20+ or browsers):

```bash
npm install @hiveio/beekeeper
```

To build bots and automation on Hive with WAX, Beekeeper and WorkerBee, see [Building agents on developers.hive.io](https://developers.hive.io/quickstart/#quickstart-building-agents).

## Building

```bash
git clone --recursive https://gitlab.syncad.com/hive/beekeeper.git
cd beekeeper
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -GNinja ..
ninja beekeeper
```

## Usage

```bash
./programs/beekeeper/beekeeper/beekeeper --webserver-http-endpoint=127.0.0.1:5001
```

Every API call carries the token of a session (`create_session`). Wallets are
opened, unlocked and locked per session: a wallet unlocked in one session stays
locked in another one until that session unlocks it with the wallet's password,
and `lock`, `lock_all`, `close`, `set_timeout` and the unlock timeout act on the
calling session only. `change_password` (a wallet the session unlocked, its current
password and a new non-empty one of at most 128 characters) re-encrypts the wallet
itself: sessions that have it unlocked keep it unlocked, every later unlock needs the
new password.

## Testing

Native unit tests (Boost.Test; configure with `-DBUILD_TESTS=ON`, run from the repository root):

```bash
./build/tests/unit/beekeeper_test
```

WASM package (Playwright):

```bash
cd programs/beekeeper/beekeeper_wasm && pnpm test
```

beekeepy (pytest):

```bash
cd python && pytest tests/beekeepy_test
```

For the containerised checks AIDEV runs, see [`.aidev/README.md`](.aidev/README.md).

## Wallet files

Wallets are stored encrypted with a key derived from the wallet password by
PBKDF2-HMAC-SHA512 (random salt; the 600,000 iterations are recorded in the
file) and authenticated with HMAC-SHA256. Files written by earlier versions,
whose key was a single unsalted SHA-512 of the password, are still read and are
converted to the current format the first time they are unlocked. The
conversion is one-way: older beekeeper versions report an invalid password for
a converted wallet, so keep a copy of the wallet directory if you may need to
go back. Wallet files are written owner-only (0600), replaced atomically and
flushed to disk.

## Dependencies

- [plugins](https://gitlab.syncad.com/hive/plugins) - Shared plugin libraries
  - [fc](https://gitlab.syncad.com/hive/fc) - Fast-compiling C++ library
  - [appbase](https://gitlab.syncad.com/hive/appbase) - Application framework

## License

MIT License - See LICENSE file.
