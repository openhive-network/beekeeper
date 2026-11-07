from __future__ import annotations

import asyncio
import threading
from typing import TYPE_CHECKING, Final

import pytest
from local_tools.beekeepy.generators import generate_wallet_name, generate_wallet_password

from beekeepy import AsyncBeekeeper, Beekeeper
from beekeepy.exceptions import InvalidPasswordError

if TYPE_CHECKING:
    from local_tools.beekeepy.models import SettingsFactory

EMPTY_PASSWORD: Final[str] = ""
UNLOCK_TIME_LIMIT_SECONDS: Final[float] = 10.0


def test_unlock_with_empty_password_raises_invalid_password_without_hanging(settings: SettingsFactory) -> None:
    # ARRANGE
    with Beekeeper.factory(settings=settings()) as bk, bk.create_session() as session:
        wallet = session.create_wallet(name=generate_wallet_name(), password=generate_wallet_password())
        wallet.lock()
        raised: list[InvalidPasswordError] = []

        def unlock_with_empty_password() -> None:
            try:
                wallet.unlock(password=EMPTY_PASSWORD)
            except InvalidPasswordError as error:
                raised.append(error)

        # a daemon thread, so a hanging unlock fails this test instead of blocking the suite
        unlocking = threading.Thread(target=unlock_with_empty_password, daemon=True)

        # ACT
        unlocking.start()
        unlocking.join(timeout=UNLOCK_TIME_LIMIT_SECONDS)

        # ASSERT
        assert not unlocking.is_alive(), f"unlock did not return within {UNLOCK_TIME_LIMIT_SECONDS}s"
        assert len(raised) == 1, "unlock with an empty password did not raise InvalidPasswordError"
        assert raised[0].wallet_name == wallet.name
        assert not wallet.is_unlocked()


async def test_unlock_of_async_wallet_with_empty_password_raises_invalid_password_without_hanging(
    settings: SettingsFactory,
) -> None:
    # ARRANGE
    async with await AsyncBeekeeper.factory(settings=settings()) as bk, await bk.create_session() as session:
        wallet = await session.create_wallet(name=generate_wallet_name(), password=generate_wallet_password())
        await wallet.lock()

        # ACT & ASSERT
        with pytest.raises(InvalidPasswordError) as raised:
            await asyncio.wait_for(wallet.unlock(password=EMPTY_PASSWORD), timeout=UNLOCK_TIME_LIMIT_SECONDS)

        assert raised.value.wallet_name == wallet.name
        assert not await wallet.is_unlocked()
