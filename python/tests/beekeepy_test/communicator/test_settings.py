from __future__ import annotations

import json
import subprocess
import sys
from datetime import timedelta
from pathlib import Path
from typing import Final

import pytest
from beekeepy.interfaces import HttpUrl
from beekeepy.settings import CommunicationSettings, InterfaceSettings, RemoteHandleSettings, RunnableHandleSettings

DURATIONS: Final[list[tuple[timedelta, str]]] = [
    (timedelta(seconds=5), "PT5S"),
    (timedelta(seconds=0.2), "PT0.2S"),
    (timedelta(hours=1), "PT3600S"),
    (timedelta(days=1, seconds=7384.5), "P1DT7384.5S"),
    (timedelta(seconds=-1.5), "-PT1.5S"),
    (timedelta(microseconds=1), "PT0.000001S"),
    (timedelta(0), "P0D"),
]
"""Times and their ISO 8601 durations in exported settings."""

FORMERLY_EXPORTED: Final[str] = (
    '{"max_retries": 5, "timeout": "PT5S", "period_between_retries": "PT0.2S", "http_endpoint": "127.0.0.1:8080",'
    ' "working_directory": "/tmp/x", "binary_path": "/bin/x", "propagate_sigint": true, "use_existing_session": "abc",'
    ' "close_timeout": "PT10S", "initialization_timeout": "PT5S"}'
)
"""Settings exported by former versions (models of msgspec)."""


def settings_of_all_members() -> InterfaceSettings:
    return InterfaceSettings(
        max_retries=3,
        timeout=timedelta(seconds=1.5),
        period_between_retries=timedelta(milliseconds=100),
        unlimited_retries_timeout=timedelta(minutes=2),
        http_endpoint=HttpUrl("https://127.0.0.1:8080"),
        working_directory=Path("/tmp/directory"),  # noqa: S108  # not created
        binary_path=Path("/bin/beekeeper"),
        propagate_sigint=False,
        use_existing_session="token",
        close_timeout=timedelta(seconds=3),
        initialization_timeout=timedelta(days=1),
    )


@pytest.mark.parametrize(
    "settings",
    [
        CommunicationSettings(max_retries=1, timeout=timedelta(seconds=2)),
        RemoteHandleSettings(http_endpoint=HttpUrl("http://127.0.0.1:8090")),
        RunnableHandleSettings(working_directory=Path("wdir"), propagate_sigint=False),
        settings_of_all_members(),
    ],
    ids=["communication", "remote-handle", "runnable-handle", "interface"],
)
def test_exported_settings_are_imported(settings: CommunicationSettings) -> None:
    # ACT
    imported = type(settings).import_settings(settings.export_settings())

    # ASSERT
    assert imported == settings


@pytest.mark.parametrize(("time", "duration"), DURATIONS, ids=[duration for _, duration in DURATIONS])
def test_times_are_exported_as_iso_8601_durations(time: timedelta, duration: str) -> None:
    # ARRANGE
    settings = CommunicationSettings(unlimited_retries_timeout=time)

    # ACT
    exported = settings.export_settings()

    # ASSERT
    assert json.loads(exported)["unlimited_retries_timeout"] == duration
    assert CommunicationSettings.import_settings(exported).unlimited_retries_timeout == time


def test_members_set_to_none_are_not_exported() -> None:
    # ARRANGE
    settings = CommunicationSettings(max_retries=5, timeout=timedelta(seconds=1.5), period_between_retries=timedelta(0))

    # ACT & ASSERT
    assert settings.export_settings() == '{"max_retries": 5, "timeout": "PT1.5S", "period_between_retries": "P0D"}'


def test_formerly_exported_settings_are_imported() -> None:
    # ACT
    settings = InterfaceSettings.import_settings(FORMERLY_EXPORTED)

    # ASSERT
    assert settings.timeout == timedelta(seconds=5)
    assert settings.period_between_retries == timedelta(seconds=0.2)
    assert settings.http_endpoint == HttpUrl("http://127.0.0.1:8080")
    assert settings.working_directory == Path("/tmp/x")  # noqa: S108  # not created
    assert settings.binary_path == Path("/bin/x")
    assert settings.use_existing_session == "abc"
    assert settings.close_timeout == timedelta(seconds=10)


@pytest.mark.parametrize(
    "exported",
    ['{"unknown": 1}', '{"max_retries": "5"}', '{"timeout": 5}', '{"timeout": "5 seconds"}', '{"propagate_sigint": 1}'],
)
def test_invalid_settings_are_not_imported(exported: str) -> None:
    # ACT & ASSERT
    with pytest.raises(ValueError):  # noqa: PT011  # messages tell the member or the value
        InterfaceSettings.import_settings(exported)


def test_copy_of_settings_is_equal_and_independent() -> None:
    # ARRANGE
    settings = settings_of_all_members()

    # ACT
    copied = settings.copy()
    copied.max_retries += 1

    # ASSERT
    assert copied != settings
    copied.max_retries -= 1
    assert copied == settings


def test_settings_and_remote_handles_do_not_load_msgspec() -> None:
    # ARRANGE - in a new interpreter, nothing imported msgspec before
    script = """
import sys
import beekeepy
from beekeepy.communication import JsonRpcRules, SchemaValidationRules
from beekeepy.handle.remote import AsyncBeekeeper, Beekeeper
from beekeepy.interfaces import HttpUrl, KeyPair
from beekeepy.settings import InterfaceSettings

settings = InterfaceSettings.import_settings(InterfaceSettings(http_endpoint=HttpUrl("127.0.0.1:1")).export_settings())
Beekeeper(settings=settings).teardown()
KeyPair(public_key="public", private_key="private")
print("msgspec" in sys.modules)
"""

    # ACT
    result = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True, check=False)

    # ASSERT
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "False"
