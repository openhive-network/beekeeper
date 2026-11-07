from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy import AsyncBeekeeper, Beekeeper
from beekeepy.exceptions import (
    BeekeeperInterfaceError,
    BeekeeperPreProcessingError,
    BeekeepyError,
    DetectableError,
    InvalidAccountNameError,
    InvalidSchemaHexError,
    InvalidSchemaPrivateKeyError,
    InvalidSchemaPublicKeyError,
    NotPositiveTimeError,
    SchemaDetectableError,
    TimeTooBigError,
)

if TYPE_CHECKING:
    from local_tools.beekeepy.models import SettingsFactory


@pytest.mark.parametrize("error", [NotPositiveTimeError, TimeTooBigError])
def test_timeout_error_hierarchy(error: type[BeekeepyError]) -> None:
    assert issubclass(error, BeekeeperPreProcessingError)
    assert issubclass(BeekeeperPreProcessingError, BeekeepyError)


@pytest.mark.parametrize(
    "error",
    [InvalidSchemaHexError, InvalidAccountNameError, InvalidSchemaPublicKeyError, InvalidSchemaPrivateKeyError],
)
def test_schema_error_hierarchy(error: type[SchemaDetectableError]) -> None:
    assert issubclass(error, BeekeeperPreProcessingError)
    assert issubclass(error, SchemaDetectableError)
    assert issubclass(error, DetectableError)
    assert issubclass(error, BeekeeperInterfaceError)
    assert issubclass(error, BeekeepyError)


@pytest.mark.parametrize(
    ("seconds", "error"), [(-20, NotPositiveTimeError), (0, NotPositiveTimeError), (2**32, TimeTooBigError)]
)
def test_invalid_timeout_is_caught_as_preprocessing_error(
    settings: SettingsFactory, seconds: int, error: type[BeekeeperPreProcessingError]
) -> None:
    with Beekeeper.factory(settings=settings()) as bk, bk.create_session() as session:
        with pytest.raises(BeekeeperPreProcessingError) as caught:
            session.set_timeout(seconds)
        assert type(caught.value) is error
        assert str(caught.value) == str(error(seconds))


@pytest.mark.parametrize(
    ("seconds", "error"), [(-20, NotPositiveTimeError), (0, NotPositiveTimeError), (2**32, TimeTooBigError)]
)
async def test_invalid_async_timeout_is_caught_as_preprocessing_error(
    settings: SettingsFactory, seconds: int, error: type[BeekeeperPreProcessingError]
) -> None:
    async with await AsyncBeekeeper.factory(settings=settings()) as bk, await bk.create_session() as session:
        with pytest.raises(BeekeeperPreProcessingError) as caught:
            await session.set_timeout(seconds)
        assert type(caught.value) is error
        assert str(caught.value) == str(error(seconds))
