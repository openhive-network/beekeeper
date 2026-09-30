# Beekeeper

Standalone wallet daemon with HTTP/WebSocket API for the Hive blockchain. Beekeeper provides secure key management and transaction signing without requiring a full blockchain node.

## Packages for consumers

| Surface | Package / binary | Docs |
|---------|------------------|------|
| Native daemon | `beekeeper` (this repo) | Build section below |
| Python | [`hiveio-beekeepy`](https://pypi.org/project/hiveio-beekeepy/) | [python/README.md](python/README.md) |
| TypeScript / JavaScript (WASM) | [`@hiveio/beekeeper`](https://www.npmjs.com/package/@hiveio/beekeeper) | [programs/beekeeper/beekeeper_wasm/README.md](programs/beekeeper/beekeeper_wasm/README.md) |

```bash
pip install hiveio-beekeepy
npm install @hiveio/beekeeper
```

## High-level documentation

- Building agents (WAX + Beekeeper + WorkerBee): [developers.hive.io — Building agents](https://developers.hive.io/quickstart/#quickstart-building-agents)
- Python usage: [python/README.md](python/README.md)
- TypeScript / WASM usage and examples: [beekeeper_wasm README](programs/beekeeper/beekeeper_wasm/README.md)

---

## Building (contributors)

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

## Dependencies

- [plugins](https://gitlab.syncad.com/hive/plugins) - Shared plugin libraries
  - [fc](https://gitlab.syncad.com/hive/fc) - Fast-compiling C++ library
  - [appbase](https://gitlab.syncad.com/hive/appbase) - Application framework

## License

MIT License - See LICENSE file.
