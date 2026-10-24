from __future__ import annotations

from typing import TYPE_CHECKING

import psutil
import pytest
from beekeepy import Beekeeper
from beekeepy.interfaces import HttpUrl
from beekeepy.settings import InterfaceSettings

if TYPE_CHECKING:
    from local_tools.beekeepy.models import SettingsFactory


def listening_address(endpoint: HttpUrl) -> str:
    """Returns the local address of the socket listening on the port of given endpoint."""
    for connection in psutil.net_connections(kind="inet4"):
        if connection.status == psutil.CONN_LISTEN and connection.laddr and connection.laddr.port == endpoint.port:
            return connection.laddr.ip
    raise AssertionError(f"Nothing listens on port {endpoint.port}")


@pytest.mark.parametrize(
    "http_endpoint",
    [
        HttpUrl("127.0.0.1:0", protocol="http"),
        HttpUrl("0.0.0.0:0", protocol="http"),
    ],
)
def test_local_beekeeper_listens_on_http_endpoint_from_settings(
    settings: SettingsFactory, http_endpoint: HttpUrl
) -> None:
    # ARRANGE & ACT
    with Beekeeper.factory(settings=settings(InterfaceSettings(http_endpoint=http_endpoint))) as bk:
        # ASSERT
        assert listening_address(bk.http_endpoint) == http_endpoint.address


def test_local_beekeeper_listens_on_loopback_by_default(settings: SettingsFactory) -> None:
    # ARRANGE & ACT
    with Beekeeper.factory(settings=settings()) as bk:
        # ASSERT
        assert listening_address(bk.http_endpoint) == "127.0.0.1"
