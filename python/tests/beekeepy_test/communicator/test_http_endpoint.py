from __future__ import annotations

import pytest
from local_tools.beekeepy.testing_server import run_simple_server

from beekeepy.handle.remote import AsyncBeekeeper, AsyncBeekeeperTemplate, Beekeeper, RemoteHandleSettings
from beekeepy.interfaces import HttpUrl


@pytest.mark.parametrize("handle_cls", [Beekeeper, AsyncBeekeeper])
async def test_endpoint_is_owned_by_each_handle(handle_cls: type[Beekeeper | AsyncBeekeeper]) -> None:
    original = HttpUrl("127.0.0.1:1")
    replacement = HttpUrl("127.0.0.1:2")
    settings = RemoteHandleSettings(http_endpoint=original)
    first = handle_cls(settings=settings)
    second = handle_cls(settings=settings)
    try:
        first.http_endpoint = replacement
        assert first.http_endpoint == replacement
        assert second.http_endpoint == original
        assert settings.http_endpoint == original
        assert first.settings.http_endpoint == original

        with first.update_settings() as updated:
            updated.http_endpoint = HttpUrl("127.0.0.1:3")
        settings.http_endpoint = HttpUrl("127.0.0.1:4")
        assert first.http_endpoint == replacement
        assert second.http_endpoint == original
    finally:
        for handle in (first, second):
            if isinstance(handle, AsyncBeekeeperTemplate):
                await handle.async_teardown()
            else:
                handle.teardown()


@pytest.mark.parametrize("handle_cls", [Beekeeper, AsyncBeekeeper])
async def test_endpoint_can_be_set_after_construction(handle_cls: type[Beekeeper | AsyncBeekeeper]) -> None:
    handle = handle_cls(settings=RemoteHandleSettings(max_retries=0))
    try:
        with pytest.raises(ValueError, match="Http endpoint is not set"):
            _ = handle.http_endpoint
        with run_simple_server('{"jsonrpc": "2.0", "id": 0, "result": {"token": "new-session"}}') as url:
            handle.http_endpoint = url
            if isinstance(handle, AsyncBeekeeperTemplate):
                session = await handle.api.create_session()
            else:
                session = handle.api.create_session()
            assert session.token == "new-session"
            assert handle.settings.http_endpoint is None
    finally:
        if isinstance(handle, AsyncBeekeeperTemplate):
            await handle.async_teardown()
        else:
            handle.teardown()
