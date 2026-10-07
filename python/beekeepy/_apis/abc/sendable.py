"""Moved to `hiveio_api._apis.abc.sendable` (hiveio-api).

Re-exported here for backward compatibility.
"""

from __future__ import annotations

from hiveio_api._apis.abc.sendable import (
    AsyncSendable,
    Sendable,
    SyncSendable,
)

__all__ = [
    "AsyncSendable",
    "Sendable",
    "SyncSendable",
]
