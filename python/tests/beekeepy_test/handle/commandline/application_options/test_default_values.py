from __future__ import annotations

import re
from typing import TYPE_CHECKING, Final

from beekeepy._executable.defaults import BeekeeperDefaults

if TYPE_CHECKING:
    from beekeepy._executable.beekeeper_config import BeekeeperConfig
    from beekeepy._executable.beekeeper_executable import BeekeeperExecutable

EXPECTED_CONFIG_OPTIONS: Final[frozenset[str]] = frozenset(
    {
        "backtrace",
        "export-keys-wallet",
        "log-json-rpc",
        "plugin",
        "unlock-interval",
        "unlock-timeout",
        "wallet-dir",
        "webserver-http-endpoint",
        "webserver-https-certificate-file-name",
        "webserver-https-endpoint",
        "webserver-https-key-file-name",
        "webserver-thread-pool-size",
        "webserver-unix-endpoint",
        "webserver-ws-deflate",
        "webserver-ws-endpoint",
    }
)

_COMMAND_LINE_ONLY_SECTION: Final[str] = "Application Command Line Options:"
_OPTION_REGEX: Final[re.Pattern[str]] = re.compile(r"^\s+--([a-z0-9][a-z0-9-]*)")


def _extract_config_option_names(help_text: str) -> set[str]:
    """Return names of options that can be set in config.ini, as listed by `--help`."""
    option_names: set[str] = set()
    in_command_line_only_section = False
    for line in help_text.splitlines():
        if line and not line[0].isspace():
            in_command_line_only_section = line.strip() == _COMMAND_LINE_ONLY_SECTION
            continue
        if not in_command_line_only_section and (match := _OPTION_REGEX.match(line)):
            option_names.add(match.group(1))
    return option_names


def _check_config_option_names(actual: set[str]) -> None:
    added = sorted(actual - EXPECTED_CONFIG_OPTIONS)
    removed = sorted(EXPECTED_CONFIG_OPTIONS - actual)
    assert actual == EXPECTED_CONFIG_OPTIONS, (
        "Set of beekeeper config options has changed, review their default values and update this test.\n"
        f"Added options: {added}\n"
        f"Removed options: {removed}"
    )


def check_default_values_from_config(default_config: BeekeeperConfig) -> None:
    assert default_config.unlock_timeout == BeekeeperDefaults.DEFAULT_UNLOCK_TIMEOUT
    assert default_config.unlock_interval == BeekeeperDefaults.DEFAULT_UNLOCK_INTERVAL
    assert default_config.log_json_rpc == BeekeeperDefaults.DEFAULT_LOG_JSON_RPC
    assert default_config.webserver_http_endpoint is None
    assert default_config.webserver_thread_pool_size == BeekeeperDefaults.DEFAULT_WEBSERVER_THREAD_POOL_SIZE
    assert default_config.backtrace == BeekeeperDefaults.DEFAULT_BACKTRACE
    assert default_config.export_keys_wallet == BeekeeperDefaults.DEFAULT_EXPORT_KEYS_WALLET


def test_config_option_names(beekeeper_exe: BeekeeperExecutable) -> None:
    """Test will check if any config option of Beekeeper was added or removed."""
    # ARRANGE & ACT
    option_names = _extract_config_option_names(beekeeper_exe.get_help_text())

    # ASSERT
    _check_config_option_names(option_names)


def test_default_values(beekeeper_exe: BeekeeperExecutable) -> None:
    """Test will check default values of Beekeeper."""
    # ARRANGE & ACT
    default_config = beekeeper_exe.generate_default_config()

    # ASSERT
    check_default_values_from_config(default_config)
