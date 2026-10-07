"""Moved to `hiveio_api._apis.abc.api_collection` (hiveio-api).

Re-exported here for backward compatibility.
"""

from __future__ import annotations

from hiveio_api._apis.abc.api_collection import (
    AbstractApiCollection,
    AbstractAsyncApiCollection,
    AbstractSyncApiCollection,
)

__all__ = [
    "AbstractApiCollection",
    "AbstractAsyncApiCollection",
    "AbstractSyncApiCollection",
]
