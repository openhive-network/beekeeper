"""Moved to `hiveio_api._apis.abc.api` (hiveio-api).

Re-exported here for backward compatibility.
"""

from __future__ import annotations

from hiveio_api._apis.abc.api import (
    AbstractApi,
    AbstractAsyncApi,
    AbstractSyncApi,
    ApiArgumentSerialization,
    ApiArgumentsToSerialize,
    HandleT,
    P,
    R,
    RegisteredApisT,
    _convert_pascal_case_to_sneak_case,
)

__all__ = [
    "AbstractApi",
    "AbstractAsyncApi",
    "AbstractSyncApi",
    "ApiArgumentSerialization",
    "ApiArgumentsToSerialize",
    "HandleT",
    "P",
    "R",
    "RegisteredApisT",
    "_convert_pascal_case_to_sneak_case",
]
