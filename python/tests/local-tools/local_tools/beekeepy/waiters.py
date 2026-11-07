from __future__ import annotations

import re
import time
from datetime import UTC, datetime
from typing import TYPE_CHECKING, Literal

from local_tools.beekeepy import checkers

if TYPE_CHECKING:
    from beekeepy.handle.runnable import Beekeeper, BeekeeperExecutable


def wait_for_beekeeper_to_close(beekeeper: Beekeeper, timeout_seconds: int = 10) -> None:
    start = datetime.now(UTC)
    while (datetime.now(UTC) - start).total_seconds() <= timeout_seconds:
        if checkers.check_for_pattern_in_file(
            beekeeper.settings.ensured_working_directory / "stderr.log", "exited cleanly"
        ):
            return
    raise TimeoutError


def wait_for_reserved_webserver_port(
    beekeeper_exe: BeekeeperExecutable, endpoint_type: Literal["HTTP", "WS"], timeout_seconds: int = 10
) -> int:
    """Return the port the webserver plugin logs as reserved for given endpoint type (HTTP covers https too)."""
    pattern = re.compile(rf"Reserved endpoint for {endpoint_type} is [0-9.]+:(\d+)")
    stderr_log = beekeeper_exe.working_directory / "stderr.log"
    start = datetime.now(UTC)
    while (datetime.now(UTC) - start).total_seconds() <= timeout_seconds:
        if stderr_log.exists() and (match := pattern.search(stderr_log.read_text(errors="replace"))):
            return int(match.group(1))
        if not beekeeper_exe.is_running():
            raise RuntimeError(f"beekeeper exited before reserving its {endpoint_type} endpoint, see {stderr_log}")
        time.sleep(0.1)
    raise TimeoutError
