# Beekeeper

Standalone wallet daemon with HTTP/WebSocket API for the Hive blockchain.

Beekeeper provides secure key management and transaction signing without requiring a full blockchain node.

## Building

```bash
git clone --recursive https://gitlab.syncad.com/hive/beekeeper.git
cd beekeeper
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc) beekeeper
```

## Usage

```bash
./beekeeper --webserver-http-endpoint=127.0.0.1:5001
```

See the [hive documentation](https://gitlab.syncad.com/hive/hive) for full usage details.

## License

See LICENSE file in the hive submodule.
