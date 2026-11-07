from __future__ import annotations

import json
from typing import TYPE_CHECKING

import pytest
import requests
from beekeepy.handle.runnable import BeekeeperArguments
from beekeepy.interfaces import HttpUrl
from hiveio_api import validate_json
from local_tools.beekeepy.network import get_port

if TYPE_CHECKING:
    from beekeepy.handle.runnable import Beekeeper


def check_webserver_http_endpoint(*, webserver_http_endpoint: HttpUrl) -> None:
    """Check if beekeeper is listening on given endpoint."""
    data = {
        "jsonrpc": "2.0",
        "method": "beekeeper_api.create_session",
        "params": {"salt": "avocado"},
        "id": 1,
    }

    resp = requests.post(webserver_http_endpoint.as_string(), data=json.dumps(data), timeout=10.0)
    assert resp.ok
    resp_json = resp.json()
    assert validate_json(resp_json["result"], "beekeeper_api.create_session") == []


@pytest.mark.parametrize("endpoint_pattern", ["0.0.0.0:{port}", "127.0.0.1:{port}"])
def test_webserver_http_endpoint(beekeeper_not_started: Beekeeper, endpoint_pattern: str) -> None:
    """Test will check command line flag --webserver_http_endpoint."""
    # ARRANGE & ACT
    webserver_http_endpoint = HttpUrl(endpoint_pattern.format(port=get_port()), protocol="http")
    beekeeper_not_started.run(
        additional_cli_arguments=BeekeeperArguments(webserver_http_endpoint=webserver_http_endpoint)
    )

    # ASSERT
    check_webserver_http_endpoint(webserver_http_endpoint=webserver_http_endpoint)
