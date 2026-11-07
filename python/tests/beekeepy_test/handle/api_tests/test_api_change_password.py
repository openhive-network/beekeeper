from __future__ import annotations

import time
from typing import TYPE_CHECKING, Final

import pytest
from beekeepy.exceptions import ErrorInResponseError, OverseerInvalidPasswordError, UnlockIsNotAccessibleError
from beekeepy.interfaces import DelayGuardBase
from local_tools.beekeepy.generators import generate_wallet_password

if TYPE_CHECKING:
    from beekeepy.handle.runnable import Beekeeper
    from local_tools.beekeepy.account_credentials import AccountCredentials
    from local_tools.beekeepy.models import WalletInfo

# After an invalid password beekeeper refuses unlock and change_password for this long.
WALLET_UNLOCK_INTERVAL: Final[float] = DelayGuardBase.BEEKEEPER_DELAY_TIME.total_seconds()


def test_api_change_password(beekeeper: Beekeeper, wallet: WalletInfo, account: AccountCredentials) -> None:
    """Test test_api_change_password will change a password and unlock the wallet only with the new one."""
    # ARRANGE
    new_password = generate_wallet_password(1)

    # ACT
    beekeeper.api.change_password(wallet_name=wallet.name, password=wallet.password, new_password=new_password)

    # ASSERT
    assert beekeeper.api.is_wallet_unlocked(wallet_name=wallet.name).unlocked is True, "Wallet should stay unlocked."
    beekeeper.api.lock(wallet_name=wallet.name)
    with pytest.raises(OverseerInvalidPasswordError):
        beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)
    time.sleep(WALLET_UNLOCK_INTERVAL)
    beekeeper.api.unlock(wallet_name=wallet.name, password=new_password)
    keys = [key.public_key for key in beekeeper.api.get_public_keys(wallet_name=wallet.name).keys]
    assert keys == [account.public_key], "Keys should be kept."


def test_api_change_password_is_saved(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_change_password_is_saved will unlock the wallet with the new password after reloading its file."""
    # ARRANGE
    new_password = generate_wallet_password(2)
    beekeeper.api.change_password(wallet_name=wallet.name, password=wallet.password, new_password=new_password)

    # ACT
    beekeeper.api.close(wallet_name=wallet.name)

    # ASSERT
    with pytest.raises(OverseerInvalidPasswordError):
        beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)
    time.sleep(WALLET_UNLOCK_INTERVAL)
    beekeeper.api.unlock(wallet_name=wallet.name, password=new_password)
    assert beekeeper.api.is_wallet_unlocked(wallet_name=wallet.name).unlocked is True, "Wallet should be unlocked."


def test_api_change_password_with_invalid_password(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_change_password_with_invalid_password will refuse a wrong current password and keep it."""
    # ARRANGE
    new_password = generate_wallet_password(3)

    # ACT & ASSERT
    with pytest.raises(OverseerInvalidPasswordError, match=f"Invalid password for wallet: {wallet.name}"):
        beekeeper.api.change_password(
            wallet_name=wallet.name, password=generate_wallet_password(4), new_password=new_password
        )
    with pytest.raises(UnlockIsNotAccessibleError):
        beekeeper.api.change_password(wallet_name=wallet.name, password=wallet.password, new_password=new_password)

    time.sleep(WALLET_UNLOCK_INTERVAL)
    beekeeper.api.lock(wallet_name=wallet.name)
    beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)


def test_api_change_password_of_locked_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_change_password_of_locked_wallet will refuse to change a password of a locked wallet."""
    # ARRANGE
    beekeeper.api.lock(wallet_name=wallet.name)

    # ACT & ASSERT
    with pytest.raises(ErrorInResponseError, match=f"Wallet is locked: {wallet.name}"):
        beekeeper.api.change_password(
            wallet_name=wallet.name, password=wallet.password, new_password=generate_wallet_password(5)
        )


@pytest.mark.parametrize(
    ("new_password", "message"),
    [("", "New password cannot be empty"), ("x" * 129, "New password is too long, maximum length is 128")],
    ids=["empty", "too-long"],
)
def test_api_change_password_to_invalid_new_password(
    beekeeper: Beekeeper, wallet: WalletInfo, new_password: str, message: str
) -> None:
    """Test test_api_change_password_to_invalid_new_password will refuse an empty or too long new password."""
    # ARRANGE & ACT & ASSERT
    with pytest.raises(ErrorInResponseError, match=message):
        beekeeper.api.change_password(wallet_name=wallet.name, password=wallet.password, new_password=new_password)

    beekeeper.api.lock(wallet_name=wallet.name)
    beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)


def test_api_change_password_to_longest_new_password(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_change_password_to_longest_new_password will accept a new password of the maximum length."""
    # ARRANGE
    new_password = "x" * 128

    # ACT
    beekeeper.api.change_password(wallet_name=wallet.name, password=wallet.password, new_password=new_password)

    # ASSERT
    beekeeper.api.lock(wallet_name=wallet.name)
    beekeeper.api.unlock(wallet_name=wallet.name, password=new_password)
