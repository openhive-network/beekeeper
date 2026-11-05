from __future__ import annotations

from typing import TYPE_CHECKING, Final

import hiveio_api_client
import pytest
from hiveio_api_client import AsyncRemoteHandle
from local_tools.beekeepy.testing_server import run_simple_server

import beekeepy.handle.remote
import beekeepy.interfaces
from beekeepy.handle.remote import AppStatusProbeAsyncApiCollection, AsyncBeekeeper, RemoteHandleSettings

if TYPE_CHECKING:
    from hiveio_api_client import AsyncSendable

    from beekeepy.interfaces import HttpUrl

HTTP_PORT: Final[int] = 8090
APP_STATUS_RESPONSE: Final[str] = (
    """{"jsonrpc": "2.0", "id": 0, "result": {"last_update": "2026-11-05T10:00:00", "records": {}, """
    """"statuses": [{"timestamp": "2026-11-05T09:59:00", "status": "beekeeper is ready"}], "webservers": """
    f"""{{"HTTP": {{"timestamp": "2026-11-05T09:58:00", "address": "127.0.0.1", "port": {HTTP_PORT}}}}}, """
    """"forks": []}}"""
)


@pytest.mark.parametrize(
    "name",
    [
        "AbstractAsyncApi",
        "AbstractAsyncApiCollection",
        "AbstractSyncApi",
        "AbstractSyncApiCollection",
        "ApiArgumentSerialization",
        "AsyncSendable",
        "RegisteredApisT",
        "SyncSendable",
    ],
)
def test_generic_names_of_remote_handles_are_objects_of_runtime_of_api_clients(name: str) -> None:
    # ASSERT
    assert getattr(beekeepy.handle.remote, name) is getattr(hiveio_api_client, name)


@pytest.mark.parametrize("name", ["AnyUrl", "HttpUrl", "P2PUrl", "Url", "WsUrl"])
def test_urls_of_interfaces_are_objects_of_runtime_of_api_clients(name: str) -> None:
    # ASSERT
    assert getattr(beekeepy.interfaces, name) is getattr(hiveio_api_client, name)


async def test_generated_api_owned_by_beekeepy_handle_calls_endpoint() -> None:
    # ARRANGE
    with run_simple_server(APP_STATUS_RESPONSE) as url:
        handle = AsyncBeekeeper(settings=RemoteHandleSettings(http_endpoint=url, max_retries=0))
        try:
            api = AppStatusProbeAsyncApiCollection(owner=handle)

            # ACT
            status = await api.app_status.get_app_status()
        finally:
            await handle.async_teardown()

    # ASSERT
    assert [item.status for item in status.statuses] == ["beekeeper is ready"]
    assert status.webservers.HTTP is not None
    assert status.webservers.HTTP.port == HTTP_PORT


async def test_beekeepy_handle_and_remote_handle_of_runtime_are_interchangeable_owners() -> None:
    # ARRANGE
    async def call_with(owner: AsyncSendable) -> object:
        return await AppStatusProbeAsyncApiCollection(owner=owner).app_status.get_app_status()

    def runtime_handle(url: HttpUrl) -> AsyncRemoteHandle[AppStatusProbeAsyncApiCollection]:
        return AsyncRemoteHandle(
            settings=hiveio_api_client.RemoteHandleSettings(http_endpoint=url, max_retries=0),
            api=AppStatusProbeAsyncApiCollection,
        )

    with run_simple_server(APP_STATUS_RESPONSE) as url:
        beekeepy_handle = AsyncBeekeeper(settings=RemoteHandleSettings(http_endpoint=url, max_retries=0))
        try:
            # ACT
            from_beekeepy_handle = await call_with(beekeepy_handle)
            from_runtime_handle = await call_with(runtime_handle(url))
        finally:
            await beekeepy_handle.async_teardown()

    # ASSERT
    assert from_beekeepy_handle == from_runtime_handle
