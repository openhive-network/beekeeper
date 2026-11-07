from __future__ import annotations

import json
import shutil
import ssl
import subprocess
from dataclasses import dataclass
from pathlib import Path  # noqa: TC003
from typing import TYPE_CHECKING

import aiohttp
import pytest
import requests
from beekeepy.handle.runnable import BeekeeperArguments
from beekeepy.interfaces import HttpUrl, WsUrl
from hiveio_api import validate_json
from local_tools.beekeepy.waiters import wait_for_reserved_webserver_port

if TYPE_CHECKING:
    from beekeepy.handle.runnable import BeekeeperExecutable


class _HttpsBeekeeperArguments(BeekeeperArguments):
    webserver_https_endpoint: HttpUrl | None = None
    webserver_https_certificate_file_name: Path | None = None
    webserver_https_key_file_name: Path | None = None


@dataclass
class _SelfSignedCertificate:
    certificate: Path
    key: Path


@pytest.fixture
def self_signed_certificate(tmp_path: Path) -> _SelfSignedCertificate:
    openssl = shutil.which("openssl")
    if openssl is None:
        pytest.skip("openssl executable is required to generate a self-signed certificate")

    certificate = tmp_path / "certificate.pem"
    key = tmp_path / "key.pem"
    subprocess.run(
        [
            openssl,
            "req",
            "-x509",
            "-newkey",
            "rsa:2048",
            "-nodes",
            "-days",
            "1",
            "-subj",
            "/CN=127.0.0.1",
            "-addext",
            "subjectAltName=IP:127.0.0.1",
            "-keyout",
            key.as_posix(),
            "-out",
            certificate.as_posix(),
        ],
        check=True,
        capture_output=True,
    )
    return _SelfSignedCertificate(certificate=certificate, key=key)


def _https_arguments(
    beekeeper_exe: BeekeeperExecutable, certificate: _SelfSignedCertificate
) -> _HttpsBeekeeperArguments:
    return _HttpsBeekeeperArguments(
        data_dir=beekeeper_exe.working_directory,
        webserver_https_endpoint=HttpUrl("127.0.0.1:0", protocol="https"),
        webserver_https_certificate_file_name=certificate.certificate,
        webserver_https_key_file_name=certificate.key,
    )


def test_webserver_https_endpoint(
    beekeeper_exe: BeekeeperExecutable, self_signed_certificate: _SelfSignedCertificate
) -> None:
    """Test will check command line flags --webserver-https-endpoint and its certificate and key files."""
    # ARRANGE
    with (
        beekeeper_exe.restore_arguments(_https_arguments(beekeeper_exe, self_signed_certificate)),
        beekeeper_exe.run(blocking=False),
    ):
        port = wait_for_reserved_webserver_port(beekeeper_exe, "HTTP")

        # ACT
        response = requests.post(
            f"https://127.0.0.1:{port}",
            data=json.dumps(
                {
                    "jsonrpc": "2.0",
                    "method": "beekeeper_api.create_session",
                    "params": {"salt": "avocado"},
                    "id": 1,
                }
            ),
            verify=self_signed_certificate.certificate.as_posix(),
            timeout=10.0,
        )

        # ASSERT
        assert response.ok
        assert validate_json(response.json()["result"], "beekeeper_api.create_session") == []


async def test_webserver_wss_endpoint_is_not_supported(
    beekeeper_exe: BeekeeperExecutable, self_signed_certificate: _SelfSignedCertificate
) -> None:
    """
    Test documents that the webserver plugin does not support wss.

    With --webserver-https-endpoint set, webserver_plugin::plugin_initialize (libraries/plugins,
    plugins/webserver/webserver_plugin.cpp) instantiates webserver_plugin_impl with a TLS server type, which is
    used for both http_server and ws_server, but start_webserver calls tls->set_tls_handlers for http_server only.
    The ws endpoint is therefore reserved, yet ws_server has no TLS init handler and a wss handshake never
    completes. Once the plugin supports wss, this test has to be turned into a positive one.
    """
    # ARRANGE
    beekeeper_exe.config.webserver_ws_endpoint = WsUrl("127.0.0.1:0", protocol="ws")
    with (
        beekeeper_exe.restore_arguments(_https_arguments(beekeeper_exe, self_signed_certificate)),
        beekeeper_exe.run(blocking=False),
    ):
        port = wait_for_reserved_webserver_port(beekeeper_exe, "WS")
        ssl_context = ssl.create_default_context(cafile=self_signed_certificate.certificate.as_posix())

        # ACT & ASSERT
        async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=3.0)) as session:
            with pytest.raises((aiohttp.ClientError, TimeoutError)):
                await session.ws_connect(f"wss://127.0.0.1:{port}", ssl=ssl_context)
