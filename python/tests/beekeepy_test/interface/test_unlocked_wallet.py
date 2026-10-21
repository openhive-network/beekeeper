from __future__ import annotations

from datetime import datetime
from typing import TYPE_CHECKING, Final

import pytest
from beekeepy import AsyncBeekeeper, Beekeeper
from beekeepy.exceptions import InvalidSchemaHexError, NotExistingKeyError
from local_tools.beekeepy.account_credentials import AccountCredentials
from local_tools.beekeepy.generators import generate_wallet_name, generate_wallet_password

if TYPE_CHECKING:
    from local_tools.beekeepy.models import SettingsFactory

NOT_HEX_DIGEST: Final[str] = "not a hex digest"


def test_digest_is_validated_before_signing(settings: SettingsFactory) -> None:
    # ARRANGE
    with Beekeeper.factory(settings=settings()) as bk, bk.create_session() as session:
        wallet = session.create_wallet(name=generate_wallet_name(), password=generate_wallet_password())
        key = wallet.import_key(private_key=AccountCredentials.create().private_key)

        # ACT & ASSERT
        with pytest.raises(InvalidSchemaHexError):
            wallet.sign_digest(sig_digest=NOT_HEX_DIGEST, key=key)


async def test_digest_is_validated_before_signing_by_async_wallet(settings: SettingsFactory) -> None:
    # ARRANGE
    async with await AsyncBeekeeper.factory(settings=settings()) as bk, await bk.create_session() as session:
        wallet = await session.create_wallet(name=generate_wallet_name(), password=generate_wallet_password())
        key = await wallet.import_key(private_key=AccountCredentials.create().private_key)

        # ACT & ASSERT
        with pytest.raises(InvalidSchemaHexError):
            await wallet.sign_digest(sig_digest=NOT_HEX_DIGEST, key=key)


def test_encrypting_with_key_not_in_wallet(settings: SettingsFactory) -> None:
    # ARRANGE
    with Beekeeper.factory(settings=settings()) as bk, bk.create_session() as session:
        wallet = session.create_wallet(name=generate_wallet_name(), password=generate_wallet_password())
        key = wallet.import_key(private_key=AccountCredentials.create().private_key)
        key_not_in_wallet = AccountCredentials.create().public_key

        # ACT & ASSERT
        with pytest.raises(NotExistingKeyError):
            wallet.encrypt_data(from_key=key_not_in_wallet, to_key=key, content="memo")


def test_lock_time_is_timeout_time_of_session(settings: SettingsFactory) -> None:
    # ARRANGE
    with Beekeeper.factory(settings=settings()) as bk, bk.create_session() as session:
        wallet = session.create_wallet(name=generate_wallet_name(), password=generate_wallet_password())

        # ACT
        lock_time = wallet.lock_time

        # ASSERT
        assert isinstance(lock_time, datetime)
        assert lock_time == session.get_info().timeout_time


async def test_lock_time_of_async_wallet_is_timeout_time_of_session(settings: SettingsFactory) -> None:
    # ARRANGE
    async with await AsyncBeekeeper.factory(settings=settings()) as bk, await bk.create_session() as session:
        wallet = await session.create_wallet(name=generate_wallet_name(), password=generate_wallet_password())

        # ACT
        lock_time = await wallet.lock_time

        # ASSERT
        assert isinstance(lock_time, datetime)
        assert lock_time == (await session.get_info()).timeout_time
