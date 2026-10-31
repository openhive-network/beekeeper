from __future__ import annotations

from datetime import timedelta
from typing import Final

import pytest
from beekeepy.settings import InterfaceSettings

# environment variable, its value, setting it sets, expected value of the setting
ENVIRONMENT: Final[list[tuple[str, str, str, object]]] = [
    ("HIVEIO_COMMUNICATION_MAX_RETRIES", "7", "max_retries", 7),
    ("HIVEIO_COMMUNICATION_TIMEOUT_SECS", "2.5", "timeout", timedelta(seconds=2.5)),
    ("HIVEIO_COMMUNICATION_PERIOD_BETWEEN_RETRIES_SECS", "0.25", "period_between_retries", timedelta(milliseconds=250)),
    ("HIVEIO_PROPAGATE_SIGINT", "false", "propagate_sigint", False),
    ("HIVEIO_CLOSE_TIMEOUT", "1.5", "close_timeout", timedelta(seconds=1.5)),
    ("HIVEIO_INITIALIZATION_TIMEOUT", "12", "initialization_timeout", timedelta(seconds=12)),
]
SETTINGS: Final[set[str]] = {setting for _, _, setting, _ in ENVIRONMENT}
FORMER_NAMES: Final[dict[str, str]] = {
    "HIVEIO_COMMUNICATION_MAX_RETRIES": "HELPY_COMMUNICATION_MAX_RETRIES",
    "HIVEIO_COMMUNICATION_TIMEOUT_SECS": "HELPY_COMMUNICATION_TIMEOUT_SECS",
    "HIVEIO_COMMUNICATION_PERIOD_BETWEEN_RETRIES_SECS": "HELPY_COMMUNICATION_PERIOD_BETWEEN_RETRIES_SECS",
    "HIVEIO_PROPAGATE_SIGINT": "BEEKEEPY_PROPAGATE_SIGINT",
    "HIVEIO_CLOSE_TIMEOUT": "BEEKEEPY_CLOSE_TIMEOUT",
    "HIVEIO_INITIALIZATION_TIMEOUT": "BEEKEEPY_INITIALIZATION_TIMEOUT",
}
"""Former (deprecated) names of environment variables."""


@pytest.fixture(autouse=True)
def _without_settings_in_environment(monkeypatch: pytest.MonkeyPatch) -> None:
    for variable, *_ in ENVIRONMENT:
        monkeypatch.delenv(variable, raising=False)
        monkeypatch.delenv(FORMER_NAMES[variable], raising=False)


@pytest.mark.parametrize(
    ("variable", "value", "setting", "expected"), ENVIRONMENT, ids=[variable for variable, *_ in ENVIRONMENT]
)
def test_environment_variable_sets_only_its_setting(
    monkeypatch: pytest.MonkeyPatch, variable: str, value: str, setting: str, expected: object
) -> None:
    # ARRANGE
    defaults = InterfaceSettings()
    assert getattr(defaults, setting) != expected, "the expected value has to differ from the default"
    monkeypatch.setenv(variable, value)

    # ACT
    settings = InterfaceSettings()

    # ASSERT
    assert getattr(settings, setting) == expected
    others = SETTINGS - {setting}
    assert {other: getattr(settings, other) for other in others} == {
        other: getattr(defaults, other) for other in others
    }


@pytest.mark.parametrize(
    ("variable", "value", "setting", "expected"), ENVIRONMENT, ids=[variable for variable, *_ in ENVIRONMENT]
)
def test_former_name_of_environment_variable_sets_its_setting_with_warning(
    monkeypatch: pytest.MonkeyPatch, variable: str, value: str, setting: str, expected: object
) -> None:
    # ARRANGE
    monkeypatch.setenv(FORMER_NAMES[variable], value)

    # ACT
    with pytest.warns(DeprecationWarning, match=variable):
        settings = InterfaceSettings()

    # ASSERT
    assert getattr(settings, setting) == expected


@pytest.mark.parametrize(
    ("variable", "value", "setting", "expected"), ENVIRONMENT, ids=[variable for variable, *_ in ENVIRONMENT]
)
def test_environment_variable_takes_precedence_over_its_former_name(
    monkeypatch: pytest.MonkeyPatch, variable: str, value: str, setting: str, expected: object
) -> None:
    # ARRANGE
    monkeypatch.setenv(variable, value)
    monkeypatch.setenv(FORMER_NAMES[variable], "invalid value")

    # ACT
    settings = InterfaceSettings()

    # ASSERT
    assert getattr(settings, setting) == expected
