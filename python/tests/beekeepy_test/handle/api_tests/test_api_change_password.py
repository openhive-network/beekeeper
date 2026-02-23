from __future__ import annotations

from typing import TYPE_CHECKING

import pytest
from beekeepy.exceptions import ErrorInResponseError, OverseerInvalidPasswordError

if TYPE_CHECKING:
    from beekeepy.handle.runnable import Beekeeper
    from local_tools.beekeepy.account_credentials import AccountCredentials
    from local_tools.beekeepy.models import WalletInfo

NEW_PASSWORD = "new_password"  # noqa: S105


def test_api_change_password(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test that change_password changes the wallet password successfully."""
    # ACT
    beekeeper.api.change_password(wallet_name=wallet.name, password=wallet.password, new_password=NEW_PASSWORD)

    # ASSERT - wallet should still be unlocked
    bk_wallet = (beekeeper.api.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is True, "Wallet should remain unlocked after password change."


def test_api_change_password_then_unlock_with_new(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test that after changing password, the new password can be used to unlock."""
    # ARRANGE
    beekeeper.api.change_password(wallet_name=wallet.name, password=wallet.password, new_password=NEW_PASSWORD)

    # ACT
    beekeeper.api.lock(wallet_name=wallet.name)
    beekeeper.api.unlock(wallet_name=wallet.name, password=NEW_PASSWORD)

    # ASSERT
    bk_wallet = (beekeeper.api.list_wallets()).wallets[0]
    assert bk_wallet.unlocked is True, "Wallet should be unlocked with new password."


def test_api_change_password_old_password_fails(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test that after changing password, the old password no longer works."""
    # ARRANGE
    beekeeper.api.change_password(wallet_name=wallet.name, password=wallet.password, new_password=NEW_PASSWORD)
    beekeeper.api.lock(wallet_name=wallet.name)

    # ACT & ASSERT
    with pytest.raises(OverseerInvalidPasswordError, match="Invalid password for wallet"):
        beekeeper.api.unlock(wallet_name=wallet.name, password=wallet.password)


def test_api_change_password_with_wrong_old_password(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test that change_password fails when the old password is incorrect."""
    # ACT & ASSERT
    with pytest.raises(OverseerInvalidPasswordError, match="Invalid password for wallet"):
        beekeeper.api.change_password(
            wallet_name=wallet.name,
            password="wrong_password",  # noqa: S106
            new_password=NEW_PASSWORD,
        )


def test_api_change_password_on_locked_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test that change_password fails when the wallet is locked."""
    # ARRANGE
    beekeeper.api.lock(wallet_name=wallet.name)

    # ACT & ASSERT
    with pytest.raises(ErrorInResponseError, match=f"Wallet is locked: {wallet.name}"):
        beekeeper.api.change_password(wallet_name=wallet.name, password=wallet.password, new_password=NEW_PASSWORD)


def test_api_change_password_preserves_keys(
    beekeeper: Beekeeper,
    wallet: WalletInfo,
    account: AccountCredentials,  # noqa: ARG001
) -> None:
    """Test that keys are preserved after changing the wallet password."""
    # ARRANGE
    keys_before = beekeeper.api.get_public_keys(wallet_name=wallet.name)

    # ACT
    beekeeper.api.change_password(wallet_name=wallet.name, password=wallet.password, new_password=NEW_PASSWORD)

    # ASSERT - keys should be the same
    keys_after = beekeeper.api.get_public_keys(wallet_name=wallet.name)
    assert keys_before == keys_after, "Keys should be preserved after password change."

    # Also verify keys survive lock/unlock cycle with new password
    beekeeper.api.lock(wallet_name=wallet.name)
    beekeeper.api.unlock(wallet_name=wallet.name, password=NEW_PASSWORD)
    keys_after_reopen = beekeeper.api.get_public_keys(wallet_name=wallet.name)
    assert keys_before == keys_after_reopen, "Keys should be preserved after lock/unlock with new password."
