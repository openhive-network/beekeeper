# Beekeepy

A high-level Python interface for interacting with a Beekeeper instance - secure Hive key management and transaction signing without a full blockchain node.

[![PyPI version](https://img.shields.io/pypi/v/hiveio-beekeepy.svg)](https://pypi.org/project/hiveio-beekeepy/)
[![CI](https://gitlab.syncad.com/hive/beekeeper/badges/master/pipeline.svg)](https://gitlab.syncad.com/hive/beekeeper/-/pipelines)

---

## Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Features](#features)
4. [Object-Oriented Interface](#object-oriented-interface)
5. [Error Handling](#error-handling)
6. [Beekeeper Instance Management](#beekeeper-instance-management)
7. [Notes on Configuration](#notes-on-configuration)
8. [Environment Variables](#environment-variables)
9. [Related Packages](#related-packages)

## Introduction

Beekeepy is a Python library designed to offer a high-level object-oriented interface for interacting with a Beekeeper instance. Whether you want to start your own Beekeeper instance or connect to an existing one, Beekeepy simplifies the process while providing robust error handling and IDE integration through comprehensive type hinting.

For building bots and automation on Hive (WAX + Beekeeper + WorkerBee), see [Building agents on developers.hive.io](https://developers.hive.io/quickstart/#quickstart-building-agents).

TypeScript / JavaScript consumers should use [`@hiveio/beekeeper`](https://www.npmjs.com/package/@hiveio/beekeeper) - see its [README](https://gitlab.syncad.com/hive/beekeeper/-/blob/master/programs/beekeeper/beekeeper_wasm/README.md).

## Installation

Requires Python 3.12 or newer on Linux (the embedded Beekeeper binary is Linux-only).

The package is published to [PyPI](https://pypi.org/project/hiveio-beekeepy/) as `hiveio-beekeepy` and imported as `beekeepy`:

```bash
pip install --extra-index-url https://gitlab.syncad.com/api/v4/groups/136/-/packages/pypi/simple hiveio-beekeepy
```

The extra index is required: the dependencies `hiveio-api-client` and `hiveio-api` (in the versions beekeepy requires) are published only in the Hive GitLab package registry, so a plain `pip install hiveio-beekeepy` cannot resolve them. The same registry also hosts development builds of `hiveio-beekeepy`.

## Features

- **Object-Oriented Interface**: Beekeepy provides a high-level, intuitive API for interacting with Beekeeper instances, allowing developers to manage and query Beekeeper with ease.
- **Synchronous and Asynchronous Support**: Beekeepy offers both synchronous and asynchronous versions of its API, allowing for flexible integration in projects using traditional blocking code or `asyncio` for non-blocking I/O.
- **Instance Management**: Beekeepy can either launch a new Beekeeper instance or connect to an already running instance.
- **IDE-Friendly with Full Typing**: Built with complete type annotations, Beekeepy is designed to integrate smoothly with modern IDEs, enhancing code navigation, auto-completion, and static analysis.
- **Exception Handling**: Beekeepy has built-in handling for common errors that may occur during interaction with Beekeeper. These exceptions are easy to catch and handle through dedicated exception classes.
- **No External Configuration Needed**: Beekeeper's binary is embedded directly into the package, making it ready to use out-of-the-box without additional configuration (Linux-only; Windows support is not planned).

## Object-Oriented Interface

Beekeepy offers a high-level, object-oriented interface, allowing you to interact with Beekeeper instances through Python objects. This simplifies many operations and reduces the complexity of integrating Beekeeper functionality into your applications.

Example usage:

```python
from beekeepy import Beekeeper, AsyncBeekeeper

# Connect to an existing instance (works on both sync and async)
async with await AsyncBeekeeper.remote_factory("http://localhost:8080") as beekeeper,
    await beekeeper.create_session() as session,
    await session.create_wallet(name="my_wallet", password="password") as wallet:
        public_key = await wallet.import_key(private_key=my_private_key)
        await wallet.sign_digest(digest=my_digest, public_key=public_key)

# Or create new local instance (works on both sync and async)
with Beekeeper.factory() as beekeeper,
    beekeeper.create_session() as session,
    session.create_wallet(name="my_wallet", password="password") as wallet:
        public_key = wallet.import_key(private_key=my_private_key)
        wallet.sign_digest(digest=my_digest, public_key=public_key)
```

With Beekeepy, you can manage Beekeeper processes easily, whether you're working with a local or remote instance.

## Error Handling

Beekeepy includes custom exception classes that handle common errors when interacting with Beekeeper. This allows you to catch and manage exceptions more effectively.

Example:

```python
from beekeepy import Beekeeper
from beekeepy.exceptions import InvalidPrivateKeyError

with Beekeeper.factory() as beekeeper,
    beekeeper.create_session() as session,
    session.create_wallet(name="my_wallet", password="password") as wallet:
        try:
            public_key = wallet.import_key(private_key="INVALID_PRIVATE_KEY")
        except InvalidPrivateKeyError as ex:
            handle_invalid_private_key()
```

Common errors such as connection issues, invalid operations, or timeout errors are wrapped in dedicated exception classes for better error handling and more readable code.

## Beekeeper Instance Management

Beekeepy is versatile in how it manages Beekeeper instances:

- **Launch a New Instance**: Start a Beekeeper instance directly from Python. The binary is bundled in the package, so no external setup is needed.
- **Connect to an Existing Instance**: Beekeepy allows you to connect to a Beekeeper instance that is already running, making it easy to integrate with existing infrastructure.

Example:

```python
# Launch a new instance of beekeeper and connect
client = await AsyncBeekeeper.factory()
# ...
await client.teardown()

# Or connect to an existing instance
client = Beekeeper.remote_factory("http://localhost:8080")
# ....
client.teardown()
```

> :warning: **Note**: If you use Beekeeper without `with` statement, remember to call `teardown` when you finish using it, so beekeeper will be properly close

## Notes on Configuration

Beekeepy comes with an embedded Beekeeper binary, so no additional configuration or setup is required to run the Beekeeper instance. Simply install the package, and you're ready to go!

> :warning: **Note**: The Beekeeper binary is compiled for Linux systems. There are no plans to support Windows.

## Environment Variables

Defaults of settings (`beekeepy.settings`) can be given by environment variables - they are read when settings are created. Times are given in seconds (also fractional, e.g. `0.5`).

| Variable                                           | Setting                  | Former name (deprecated)                          |
|----------------------------------------------------|--------------------------|---------------------------------------------------|
| `HIVEIO_COMMUNICATION_MAX_RETRIES`                 | `max_retries`            | `HELPY_COMMUNICATION_MAX_RETRIES`                 |
| `HIVEIO_COMMUNICATION_TIMEOUT_SECS`                | `timeout`                | `HELPY_COMMUNICATION_TIMEOUT_SECS`                |
| `HIVEIO_COMMUNICATION_PERIOD_BETWEEN_RETRIES_SECS` | `period_between_retries` | `HELPY_COMMUNICATION_PERIOD_BETWEEN_RETRIES_SECS` |
| `HIVEIO_PROPAGATE_SIGINT`                          | `propagate_sigint`       | `BEEKEEPY_PROPAGATE_SIGINT`                       |
| `HIVEIO_CLOSE_TIMEOUT`                             | `close_timeout`          | `BEEKEEPY_CLOSE_TIMEOUT`                          |
| `HIVEIO_INITIALIZATION_TIMEOUT`                    | `initialization_timeout` | `BEEKEEPY_INITIALIZATION_TIMEOUT`                 |

Former names are still read, with `DeprecationWarning`, when the variable of the new name is not set. Each of them now sets the setting of its name - formerly the `HELPY_COMMUNICATION_*` variables set wrong settings (e.g. `HELPY_COMMUNICATION_MAX_RETRIES` set the timeout).

## Related Packages

| Package                                                                | Role                                                |
|------------------------------------------------------------------------|-----------------------------------------------------|
| [`@hiveio/beekeeper`](https://www.npmjs.com/package/@hiveio/beekeeper) | Beekeeper for TypeScript / JavaScript (WASM)        |
| [`hiveio-wax`](https://pypi.org/project/hiveio-wax/)                   | Hive protocol and transaction building (Python)     |
| [`@hiveio/wax`](https://www.npmjs.com/package/@hiveio/wax)             | Hive protocol and transaction building (TypeScript) |
| [`@hiveio/workerbee`](https://www.npmjs.com/package/@hiveio/workerbee) | Observing and automating Hive (TypeScript)          |

How they fit together: [Building agents on developers.hive.io](https://developers.hive.io/quickstart/#quickstart-building-agents).
