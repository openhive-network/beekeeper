from __future__ import annotations

import re
from datetime import datetime
from typing import TYPE_CHECKING, Final

import pytest
from beekeepy import AsyncBeekeeper, Beekeeper
from beekeepy._interface.validators import (
    validate_account_names,
    validate_digest,
    validate_private_keys,
    validate_public_keys,
)
from beekeepy.exceptions import (
    InvalidAccountNameError,
    InvalidSchemaHexError,
    InvalidSchemaPrivateKeyError,
    InvalidSchemaPublicKeyError,
    NotExistingKeyError,
)
from local_tools.beekeepy.account_credentials import AccountCredentials
from local_tools.beekeepy.generators import generate_wallet_name, generate_wallet_password

if TYPE_CHECKING:
    from collections.abc import Callable

    from beekeepy.exceptions import SchemaDetectableError
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


INVALID_PRIVATE_KEY: Final[str] = "invalid"
INVALID_PUBLIC_KEY: Final[str] = "xyz"


def test_keys_are_validated_before_sending(settings: SettingsFactory) -> None:
    # ARRANGE
    with Beekeeper.factory(settings=settings()) as bk, bk.create_session() as session:
        wallet = session.create_wallet(name=generate_wallet_name(), password=generate_wallet_password())
        key = wallet.import_key(private_key=AccountCredentials.create().private_key)

        # ACT & ASSERT
        with pytest.raises(InvalidSchemaPrivateKeyError, match=INVALID_PRIVATE_KEY):
            wallet.import_key(private_key=INVALID_PRIVATE_KEY)
        with pytest.raises(InvalidSchemaPublicKeyError, match=INVALID_PUBLIC_KEY):
            wallet.sign_digest(sig_digest=NOT_HEX_DIGEST, key=INVALID_PUBLIC_KEY)
        assert wallet.public_keys == [key], "invalid keys are not sent"


async def test_keys_are_validated_before_sending_by_async_wallet(settings: SettingsFactory) -> None:
    # ARRANGE
    async with await AsyncBeekeeper.factory(settings=settings()) as bk, await bk.create_session() as session:
        wallet = await session.create_wallet(name=generate_wallet_name(), password=generate_wallet_password())
        key = await wallet.import_key(private_key=AccountCredentials.create().private_key)

        # ACT & ASSERT
        with pytest.raises(InvalidSchemaPrivateKeyError, match=INVALID_PRIVATE_KEY):
            await wallet.import_key(private_key=INVALID_PRIVATE_KEY)
        with pytest.raises(InvalidSchemaPublicKeyError, match=INVALID_PUBLIC_KEY):
            await wallet.sign_digest(sig_digest=NOT_HEX_DIGEST, key=INVALID_PUBLIC_KEY)
        assert await wallet.public_keys == [key], "invalid keys are not sent"


@pytest.mark.parametrize(
    ("validate", "error", "value"),
    [
        (validate_private_keys, InvalidSchemaPrivateKeyError, INVALID_PRIVATE_KEY),
        (validate_public_keys, InvalidSchemaPublicKeyError, INVALID_PUBLIC_KEY),
        (validate_account_names, InvalidAccountNameError, "Invalid Name!"),
        (validate_digest, InvalidSchemaHexError, NOT_HEX_DIGEST),
    ],
    ids=["private-key", "public-key", "account-name", "digest"],
)
def test_validators_reject_invalid_values_naming_the_argument(
    validate: Callable[..., None], error: type[SchemaDetectableError], value: str
) -> None:
    # ACT & ASSERT
    with pytest.raises(error, match=f"`argument` .*`{re.escape(value)}`"):
        validate(argument=value)


@pytest.mark.parametrize(
    ("validate", "value"),
    [
        (validate_private_keys, "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"),
        (validate_public_keys, "STM6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4"),
        (validate_account_names, "alice"),
        (validate_digest, "9b29ba0710af3918e81d7b935556d7ab205d8a8f5ca2e2427535980c2e8bdaff"),
    ],
    ids=["private-key", "public-key", "account-name", "digest"],
)
def test_validators_accept_valid_values(validate: Callable[..., None], value: str) -> None:
    # ACT & ASSERT
    validate(argument=value)


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
