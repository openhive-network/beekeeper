# Beekeeper

Standalone wallet daemon with HTTP/WebSocket API for the Hive blockchain.

Beekeeper provides secure key management and transaction signing without requiring a full blockchain node.

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

## Dependencies

- [plugins](https://gitlab.syncad.com/hive/plugins) - Shared plugin libraries
  - [fc](https://gitlab.syncad.com/hive/fc) - Fast-compiling C++ library
  - [appbase](https://gitlab.syncad.com/hive/appbase) - Application framework

## License

MIT License - See LICENSE file.
