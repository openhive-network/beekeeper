# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Beekeeper is a standalone wallet daemon with HTTP/WebSocket API for the Hive blockchain. It provides secure key management and transaction signing without requiring a full blockchain node.

Two builds exist:
- **Native C++**: HTTP/WebSocket server daemon
- **WASM**: TypeScript/JavaScript bindings (`@hiveio/beekeeper` npm package)

## Build Commands

### Native Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -GNinja ..
ninja                    # Build everything including tests
ninja beekeeper         # Build daemon only
```

Executable: `build/programs/beekeeper/beekeeper/beekeeper`

### WASM Build

Requires Docker with Emscripten image or the CI environment.

```bash
# Build WASM module (from repo root)
./scripts/build_wasm_beekeeper.sh 1 "$(pwd)"

# Build TypeScript package
cd programs/beekeeper/beekeeper_wasm
pnpm install --frozen-lockfile
pnpm build
```

### Running the Daemon

```bash
./build/programs/beekeeper/beekeeper/beekeeper --webserver-http-endpoint=127.0.0.1:5001
```

## Testing

### Native Tests (Boost.Test)

```bash
./build/tests/unit/beekeeper_test --log_level=test_suite

# Run specific test
./build/tests/unit/beekeeper_test --run_test=test_name
```

### WASM Tests (Playwright)

```bash
cd programs/beekeeper/beekeeper_wasm
pnpm exec playwright install chromium
pnpm test
```

## Architecture

```
programs/beekeeper/
├── beekeeper/           # Native daemon (HTTP/WebSocket API)
│   ├── main.cpp
│   ├── beekeeper_wallet_api.cpp   # JSON-RPC API endpoints
│   ├── beekeeper_instance.cpp     # Instance management
│   ├── time_manager.cpp           # Auto-lock timeout
│   └── session_manager.cpp        # Session lifecycle
├── core/                # Shared C++ wallet management library
│   ├── beekeeper_wallet_manager.cpp
│   ├── wallet_content_handler.cpp  # Encryption/decryption
│   ├── session_manager_base.cpp
│   └── time_manager_base.cpp
├── beekeeper_wasm/      # WASM bindings + TypeScript package
│   ├── beekeeper_wasm_api.cpp     # C++ WASM wrapper
│   ├── src/detailed/
│   │   ├── api.ts                 # BeekeeperApi class
│   │   ├── session.ts
│   │   ├── wallet.ts
│   │   └── interfaces.ts          # Type definitions
│   └── __tests__/                 # Playwright tests
└── examples/            # Nuxt SSR, SharedWorker examples
```

**Shared Core Pattern**: The `core/` library contains wallet management logic used by both native and WASM builds.

## Key Dependencies

- **plugins** (git submodule): fc, appbase, webserver_plugin, json_rpc
- **npm-common-config** (git submodule): TypeScript build tooling

Both submodules are required. Clone with `--recursive` or run `git submodule update --init --recursive`.

## CI Pipeline

GitLab CI runs four jobs:
- `beekeeper_native_build` → `beekeeper_native_test`
- `beekeeper_wasm_build` → `beekeeper_wasm_test`

Native and WASM builds run in parallel.
