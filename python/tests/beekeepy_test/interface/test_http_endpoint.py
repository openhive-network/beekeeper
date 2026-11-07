from __future__ import annotations

from typing import TYPE_CHECKING

import psutil
import pytest
from beekeepy import AsyncBeekeeper, Beekeeper
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


def test_packed_beekeeper_preserves_active_endpoint(settings: SettingsFactory) -> None:
    with Beekeeper.factory(settings=settings()) as bk:
        endpoint = bk.http_endpoint
        assert endpoint.port != 0
        with bk.update_settings() as updated:
            assert isinstance(updated, InterfaceSettings)
            updated.http_endpoint = HttpUrl("127.0.0.1:1")
        assert bk.http_endpoint == endpoint
        with bk.pack().unpack() as remote:
            assert remote.http_endpoint == endpoint
            info = remote.create_session().get_info()
            assert info.timeout_time > info.now


async def test_async_packed_beekeeper_preserves_active_endpoint(settings: SettingsFactory) -> None:
    async with await AsyncBeekeeper.factory(settings=settings()) as bk:
        endpoint = bk.http_endpoint
        assert endpoint.port != 0
        with bk.update_settings() as updated:
            assert isinstance(updated, InterfaceSettings)
            updated.http_endpoint = HttpUrl("127.0.0.1:1")
        assert bk.http_endpoint == endpoint
        async with await bk.pack().unpack() as remote:
            assert remote.http_endpoint == endpoint
            session = await remote.create_session()
            info = await session.get_info()
            assert info.timeout_time > info.now
