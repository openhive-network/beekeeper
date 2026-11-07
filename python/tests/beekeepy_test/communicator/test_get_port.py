from __future__ import annotations

import socket
from typing import TYPE_CHECKING, Final

import pytest
from local_tools.beekeepy.network import get_port

if TYPE_CHECKING:
    from pathlib import Path

ANY_PORT: Final[range] = range(1, 65536)


def assert_bindable(port: int) -> None:
    with socket.socket() as sock:
        sock.bind(("", port))


@pytest.mark.parametrize(
    ("ephemeral_port_range", "expected_ports"),
    [
        ("1024 65535", [ANY_PORT]),
        ("10050\t65535", [range(10000, 10050)]),
        ("1024\t65000", [range(65001, 65536)]),
        ("32768\t60999", [range(10000, 32768), range(61000, 65536)]),
    ],
)
def test_get_port_with_ephemeral_range(
    working_directory: Path, ephemeral_port_range: str, expected_ports: list[range]
) -> None:
    # ARRANGE
    range_file = working_directory / "ip_local_port_range"
    range_file.write_text(f"{ephemeral_port_range}\n")

    # ACT
    port = get_port(range_file)

    # ASSERT
    assert any(port in ports for ports in expected_ports)
    assert_bindable(port)


@pytest.mark.parametrize("content", [None, "", "not a range"])
def test_get_port_without_readable_ephemeral_range(working_directory: Path, content: str | None) -> None:
    # ARRANGE
    range_file = working_directory / "ip_local_port_range"
    if content is not None:
        range_file.write_text(content)

    # ACT
    port = get_port(range_file)

    # ASSERT
    assert port in ANY_PORT
    assert_bindable(port)
