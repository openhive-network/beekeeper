from __future__ import annotations

from datetime import timedelta
from functools import wraps
from typing import TYPE_CHECKING, Final

import pytest
from beekeepy.settings import InterfaceSettings
from loguru import logger

if TYPE_CHECKING:
    from collections.abc import Iterator
    from pathlib import Path

    from local_tools.beekeepy.models import SettingsFactory, SettingsLoggerFactory
    from loguru import Logger

# Each wallet created (or unlocked) in a beekeeper runs its PBKDF2 at the production work factor (600k iterations):
# about 0.3 s of a core, which a saturated host stretched past the 5 s default. The timeout of a request guards against
# a hung beekeeper here, no test measures a request with it.
_REQUEST_TIMEOUT: Final[timedelta] = timedelta(seconds=60)


def _with_request_timeout(settings: InterfaceSettings) -> InterfaceSettings:
    """Raise the request timeout of given settings, unless they set one other than the default."""
    if settings.timeout == InterfaceSettings().timeout:
        settings.timeout = _REQUEST_TIMEOUT
    return settings


@pytest.fixture
def settings(working_directory: Path) -> SettingsFactory:
    @wraps(settings)
    def _factory(settings_update: InterfaceSettings | None = None) -> InterfaceSettings:
        amount_of_beekeepers_in_working_directory = len([x for x in working_directory.glob("Beekeeper*") if x.is_dir()])
        working_dir = working_directory / f"Beekeeper{amount_of_beekeepers_in_working_directory}"
        result = _with_request_timeout(settings_update or InterfaceSettings())
        result.working_directory = working_dir
        return result

    return _factory


@pytest.fixture
def settings_with_logger(request: pytest.FixtureRequest, settings: SettingsFactory) -> Iterator[SettingsLoggerFactory]:
    handlers_to_remove: list[int] = []

    @wraps(settings_with_logger)
    def _factory(settings_update: InterfaceSettings | None = None) -> tuple[InterfaceSettings, Logger]:
        sets = settings(settings_update)
        test_name = request.node.name
        log = logger.bind(test_name=test_name)
        handlers_to_remove.append(
            log.add(
                sets.ensured_working_directory / "beekeeper.log",
                filter=lambda params: params["extra"].get("test_name") == test_name,
            )
        )
        return sets, log

    yield _factory

    for hid in handlers_to_remove:
        logger.remove(hid)
