from __future__ import annotations

import time
from typing import TYPE_CHECKING, Final

import pytest
from beekeepy.exceptions import ErrorInResponseError, OverseerInvalidPasswordError
from beekeepy.interfaces import DelayGuardBase
from local_tools.beekeepy.constants import DIGEST_TO_SIGN

if TYPE_CHECKING:
    from beekeepy.handle.runnable import Beekeeper
    from local_tools.beekeepy.account_credentials import AccountCredentials
    from local_tools.beekeepy.models import WalletInfo

# After a failed unlock the next one is accepted only after this delay.
WALLET_UNLOCK_INTERVAL: Final[float] = DelayGuardBase.BEEKEEPER_DELAY_TIME.total_seconds()


def is_unlocked(beekeeper: Beekeeper, wallet: WalletInfo, token: str | None = None) -> bool:
    return any(item.name == wallet.name and item.unlocked for item in beekeeper.api.list_wallets(token=token).wallets)


def sign(beekeeper: Beekeeper, wallet: WalletInfo, account: AccountCredentials, token: str | None = None) -> str:
    return beekeeper.api.sign_digest(
        sig_digest=DIGEST_TO_SIGN, public_key=account.public_key, wallet_name=wallet.name, token=token
    ).signature


def encrypt(beekeeper: Beekeeper, wallet: WalletInfo, account: AccountCredentials, token: str | None = None) -> str:
    return beekeeper.api.encrypt_data(
        wallet_name=wallet.name,
        from_public_key=account.public_key,
        to_public_key=account.public_key,
        content="content",
        token=token,
    ).encrypted_content


def assert_wallet_unusable(
    beekeeper: Beekeeper, wallet: WalletInfo, account: AccountCredentials, token: str, encrypted: str
) -> None:
    with pytest.raises(
        ErrorInResponseError, match=f"Public key {account.public_key} not found in {wallet.name} wallet"
    ):
        sign(beekeeper, wallet, account, token)
    with pytest.raises(ErrorInResponseError, match=f"Public key {account.public_key} not found in unlocked wallets"):
        beekeeper.api.sign_digest(sig_digest=DIGEST_TO_SIGN, public_key=account.public_key, token=token)
    with pytest.raises(ErrorInResponseError):
        beekeeper.api.get_public_keys(wallet_name=wallet.name, token=token)
    with pytest.raises(ErrorInResponseError):
        encrypt(beekeeper, wallet, account, token)
    with pytest.raises(ErrorInResponseError):
        beekeeper.api.decrypt_data(
            wallet_name=wallet.name,
            from_public_key=account.public_key,
            to_public_key=account.public_key,
            encrypted_content=encrypted,
            token=token,
        )
    with pytest.raises(ErrorInResponseError):
        beekeeper.api.import_key(wallet_name=wallet.name, wif_key=account.private_key, token=token)
    with pytest.raises(ErrorInResponseError):
        beekeeper.api.remove_key(wallet_name=wallet.name, public_key=account.public_key, token=token)


def test_api_wallet_opened_in_another_session_is_locked_there(
    beekeeper: Beekeeper, wallet: WalletInfo, account: AccountCredentials
) -> None:
    # ARRANGE
    other_session = beekeeper.api.create_session(salt="other").token
    encrypted = encrypt(beekeeper, wallet, account)

    # ACT
    beekeeper.api.open(wallet_name=wallet.name, token=other_session)

    # ASSERT
    assert is_unlocked(beekeeper, wallet), "Wallet should stay unlocked in the session that unlocked it."
    assert not is_unlocked(beekeeper, wallet, other_session), "Wallet should be locked in the session that opened it."
    assert_wallet_unusable(beekeeper, wallet, account, other_session, encrypted)
    assert not beekeeper.api.has_matching_private_key(
        wallet_name=wallet.name, public_key=account.public_key, token=other_session
    ).exists, "Locked wallet should not report its keys."
    assert account.public_key in [
        key.public_key for key in beekeeper.api.get_public_keys(wallet_name=wallet.name).keys
    ], "Key should stay in the wallet."


def test_api_wallet_not_opened_in_session_is_unusable(
    beekeeper: Beekeeper, wallet: WalletInfo, account: AccountCredentials
) -> None:
    # ARRANGE
    other_session = beekeeper.api.create_session(salt="other").token
    encrypted = encrypt(beekeeper, wallet, account)

    # ACT & ASSERT
    assert_wallet_unusable(beekeeper, wallet, account, other_session, encrypted)
    with pytest.raises(ErrorInResponseError):
        beekeeper.api.has_matching_private_key(
            wallet_name=wallet.name, public_key=account.public_key, token=other_session
        )


def test_api_unlock_in_another_session_requires_password(
    beekeeper: Beekeeper, wallet: WalletInfo, account: AccountCredentials
) -> None:
    # ARRANGE
    other_session = beekeeper.api.create_session(salt="other").token
    encrypted = encrypt(beekeeper, wallet, account)

    # ACT & ASSERT
    # Usually "Invalid password for wallet"; rarely the wrongly decrypted data fails to parse instead.
    with pytest.raises((OverseerInvalidPasswordError, ErrorInResponseError)):
        beekeeper.api.unlock(wallet_name=wallet.name, password="wrong password", token=other_session)  # noqa: S106
    assert not is_unlocked(beekeeper, wallet, other_session), "Wrong password should not unlock the wallet."
    assert_wallet_unusable(beekeeper, wallet, account, other_session, encrypted)

    time.sleep(WALLET_UNLOCK_INTERVAL)
    beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password, token=other_session)
    assert is_unlocked(beekeeper, wallet, other_session), "Wallet should be unlocked with its password."
    assert sign(beekeeper, wallet, account, other_session) == sign(beekeeper, wallet, account), "Signatures differ."


def test_api_lock_close_and_timeout_act_on_calling_session_only(
    beekeeper: Beekeeper, wallet: WalletInfo, account: AccountCredentials
) -> None:
    # ARRANGE
    other_session = beekeeper.api.create_session(salt="other").token
    beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password, token=other_session)

    # ACT & ASSERT
    beekeeper.api.lock(wallet_name=wallet.name, token=other_session)
    assert not is_unlocked(beekeeper, wallet, other_session), "Wallet should be locked in the session that locked it."
    assert is_unlocked(beekeeper, wallet), "lock should not lock the wallet in another session."

    beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password, token=other_session)
    beekeeper.api.lock_all(token=other_session)
    assert not is_unlocked(beekeeper, wallet, other_session), "Wallet should be locked by lock_all."
    assert is_unlocked(beekeeper, wallet), "lock_all should not lock the wallet in another session."

    beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password, token=other_session)
    beekeeper.api.close(wallet_name=wallet.name, token=other_session)
    assert not beekeeper.api.list_wallets(token=other_session).wallets, "Wallet should be closed in this session."
    assert is_unlocked(beekeeper, wallet), "close should not close the wallet in another session."
    sign(beekeeper, wallet, account)

    beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password, token=other_session)
    beekeeper.api.set_timeout(seconds=1, token=other_session)
    assert (
        beekeeper.api.get_info(token=other_session).timeout_time < beekeeper.api.get_info().timeout_time
    ), "Timeout should be set for the calling session only."
    time.sleep(1.5)
    assert not is_unlocked(beekeeper, wallet, other_session), "Wallet should be locked after the session timeout."
    assert is_unlocked(beekeeper, wallet), "Timeout of another session should not lock the wallet."


def test_api_close_session_locks_its_wallets(
    beekeeper: Beekeeper, wallet: WalletInfo, account: AccountCredentials
) -> None:
    # ARRANGE: only the other session has the wallet open and unlocked
    other_session = beekeeper.api.create_session(salt="other").token
    encrypted = encrypt(beekeeper, wallet, account)
    beekeeper.api.close(wallet_name=wallet.name)
    beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password, token=other_session)

    # ACT
    beekeeper.api.close_session(token=other_session)

    # ASSERT
    third_session = beekeeper.api.create_session(salt="third").token
    beekeeper.api.open(wallet_name=wallet.name, token=third_session)
    assert not is_unlocked(beekeeper, wallet, third_session), "No session should have the wallet unlocked."
    assert_wallet_unusable(beekeeper, wallet, account, third_session, encrypted)
