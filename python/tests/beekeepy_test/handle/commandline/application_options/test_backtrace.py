from __future__ import annotations

from typing import TYPE_CHECKING, Literal

import pytest
from beekeepy.handle.runnable import BeekeeperArguments
from beekeepy.interfaces import HttpUrl
from local_tools.beekeepy import checkers
from local_tools.beekeepy.waiters import wait_for_reserved_webserver_port

if TYPE_CHECKING:
    from beekeepy.handle.runnable import (
        BeekeeperExecutable,
    )


@pytest.mark.parametrize("backtrace", ["yes", "no"])
def test_backtrace(backtrace: Literal["yes", "no"], beekeeper_exe: BeekeeperExecutable) -> None:
    """Test will check command line flag --backtrace."""
    # ARRANGE & ACT
    beekeeper_exe.config.webserver_http_endpoint = HttpUrl("127.0.0.1:0")

    with (
        beekeeper_exe.restore_arguments(
            BeekeeperArguments(data_dir=beekeeper_exe.working_directory, backtrace=backtrace)
        ),
        beekeeper_exe.run(
            blocking=False,
        ),
    ):
        wait_for_reserved_webserver_port(beekeeper_exe, "HTTP")
        # ASSERT
        assert checkers.check_for_pattern_in_file(
            beekeeper_exe.working_directory / "stderr.log",
            "Backtrace on segfault is enabled.",
        ) is (backtrace == "yes")
