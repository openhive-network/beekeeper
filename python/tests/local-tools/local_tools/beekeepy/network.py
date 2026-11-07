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
_HIGHEST_PORT: Final[int] = 65535
_ATTEMPTS: Final[int] = 100


def get_port(ephemeral_port_range: Path = _EPHEMERAL_PORT_RANGE) -> int:
    """Return a free port, outside the kernel's ephemeral range when there is room for it.

    A port of the ephemeral range (what binding to port 0 returns) is not reserved once its socket is closed: every
    beekeeper started on `127.0.0.1:0` and every outgoing connection takes ports from that range, so another test can
    take it before the caller binds it. Outside that range only explicit binds take a port.

    `ephemeral_port_range` is the file holding the range as Linux `ip_local_port_range` does. When it cannot be read,
    or no port outside the range is free, the port is one the kernel picks for port 0.
    """
    for port in _ports_outside_ephemeral_range(ephemeral_port_range):
        with socket.socket() as sock:
            try:
                sock.bind(("", port))
            except OSError:
                continue
        return port
    with socket.socket() as sock:
        sock.bind(("", 0))
        return int(sock.getsockname()[1])


def _ports_outside_ephemeral_range(ephemeral_port_range: Path) -> list[int]:
    try:
        first_ephemeral_port, last_ephemeral_port = map(int, ephemeral_port_range.read_text().split())
    except (OSError, ValueError):
        return []
    candidates = [
        *range(_LOWEST_PORT, first_ephemeral_port),
        *range(max(_LOWEST_PORT, last_ephemeral_port + 1), _HIGHEST_PORT + 1),
    ]
    return random.sample(candidates, min(_ATTEMPTS, len(candidates)))
