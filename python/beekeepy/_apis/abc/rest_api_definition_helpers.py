"""Moved to `hiveio_api._apis.abc.rest_api_definition_helpers` (hiveio-api).

Re-exported here for backward compatibility.
"""

from __future__ import annotations

from hiveio_api._apis.abc.rest_api_definition_helpers import (
    AsyncRestApiDefinitionHelper,
    P,
    R,
    SyncRestApiDefinitionHelper,
)

__all__ = [
    "AsyncRestApiDefinitionHelper",
    "P",
    "R",
    "SyncRestApiDefinitionHelper",
]
