from __future__ import annotations

import random
import socket
from json import loads
from pathlib import Path
from typing import TYPE_CHECKING, Any, Final

from beekeepy.communication import get_communicator_cls
from beekeepy.settings import RemoteHandleSettings

if TYPE_CHECKING:
    from beekeepy.interfaces import HttpUrl
    from hiveio_api_client import JSONRPCRequest


async def async_raw_http_call(*, http_endpoint: HttpUrl, data: JSONRPCRequest) -> dict[str, Any]:
    """Make raw call with given data to given http_endpoint."""
    communicator = get_communicator_cls("async")(settings=RemoteHandleSettings(http_endpoint=http_endpoint))
    response = await communicator.async_post(url=http_endpoint, data=data.json())
    parsed = loads(response)
    assert isinstance(parsed, dict), "expected json object"
    return parsed


def raw_http_call(*, http_endpoint: HttpUrl, data: JSONRPCRequest) -> dict[str, Any]:
    """Make raw call with given data to given http_endpoint."""
    communicator = get_communicator_cls("sync")(settings=RemoteHandleSettings(http_endpoint=http_endpoint))
    response = communicator.post(url=http_endpoint, data=data.json())
    parsed = loads(response)
    assert isinstance(parsed, dict), "expected json object"
    return parsed


_EPHEMERAL_PORT_RANGE: Final[Path] = Path("/proc/sys/net/ipv4/ip_local_port_range")
_LOWEST_PORT: Final[int] = 10000
_ATTEMPTS: Final[int] = 100


def get_port() -> int:
    """Return a port that is free and below the kernel's ephemeral range.

    A port of the ephemeral range (what binding to port 0 returns) is not reserved once its socket is closed: every
    beekeeper started on `127.0.0.1:0` and every outgoing connection takes ports from that range, so another test can
    take it before the caller binds it. Below that range only explicit binds take a port.
    """
    first_ephemeral_port = int(_EPHEMERAL_PORT_RANGE.read_text().split()[0])
    for port in random.sample(range(_LOWEST_PORT, first_ephemeral_port), _ATTEMPTS):
        with socket.socket() as sock:
            try:
                sock.bind(("", port))
            except OSError:
                continue
        return port
    raise RuntimeError(f"no free port among {_ATTEMPTS} tried below {first_ephemeral_port}")
