"""Moved to `hiveio_api._communication.url` (hiveio-api).

Re-exported here for backward compatibility.
"""

from __future__ import annotations

from hiveio_api._communication.url import (
    AnyUrl,
    HttpProtocolT,
    HttpUrl,
    P2PProtocolT,
    P2PUrl,
    ProtocolT,
    Url,
    WsProtocolT,
    WsUrl,
)

__all__ = [
    "AnyUrl",
    "HttpProtocolT",
    "HttpUrl",
    "P2PProtocolT",
    "P2PUrl",
    "ProtocolT",
    "Url",
    "WsProtocolT",
    "WsUrl",
]
