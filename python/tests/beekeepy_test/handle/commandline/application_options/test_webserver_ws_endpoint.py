from __future__ import annotations

import json
from typing import TYPE_CHECKING

import aiohttp
from beekeepy.handle.runnable import BeekeeperArguments
from beekeepy.interfaces import WsUrl
from hiveio_api import validate_json
from local_tools.beekeepy.waiters import wait_for_reserved_webserver_port

if TYPE_CHECKING:
    from beekeepy.handle.runnable import BeekeeperExecutable


async def test_webserver_ws_endpoint(beekeeper_exe: BeekeeperExecutable) -> None:
    """Test will check config option webserver-ws-endpoint."""
    # ARRANGE
    beekeeper_exe.config.webserver_ws_endpoint = WsUrl("127.0.0.1:0", protocol="ws")
    with (
        beekeeper_exe.restore_arguments(BeekeeperArguments(data_dir=beekeeper_exe.working_directory)),
        beekeeper_exe.run(blocking=False),
    ):
        port = wait_for_reserved_webserver_port(beekeeper_exe, "WS")

        # ACT
        async with (
            aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=10.0)) as session,
            session.ws_connect(f"ws://127.0.0.1:{port}") as websocket,
        ):
            await websocket.send_str(
                json.dumps(
                    {
                        "jsonrpc": "2.0",
                        "method": "beekeeper_api.create_session",
                        "params": {"salt": "avocado"},
                        "id": 1,
                    }
                )
            )
            response = json.loads(await websocket.receive_str())

    # ASSERT
    assert validate_json(response["result"], "beekeeper_api.create_session") == []
