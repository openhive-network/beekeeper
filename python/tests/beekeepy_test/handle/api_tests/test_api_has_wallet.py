from __future__ import annotations

from typing import TYPE_CHECKING

from local_tools.beekeepy.generators import generate_wallet_name, generate_wallet_password

if TYPE_CHECKING:
    from beekeepy.handle.runnable import Beekeeper
    from local_tools.beekeepy.models import WalletInfo


def test_api_has_wallet(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_has_wallet will test beekeeper_api.has_wallet api call."""
    # ACT & ASSERT
    assert beekeeper.api.has_wallet(wallet_name=wallet.name).exists
    assert not beekeeper.api.has_wallet(wallet_name=generate_wallet_name(1)).exists


def test_api_is_wallet_unlocked(beekeeper: Beekeeper, wallet: WalletInfo) -> None:
    """Test test_api_is_wallet_unlocked will test beekeeper_api.is_wallet_unlocked api call."""
    # ARRANGE
    assert beekeeper.api.is_wallet_unlocked(wallet_name=wallet.name).unlocked, "Created wallet is unlocked."

    # ACT
    beekeeper.api.lock(wallet_name=wallet.name)

    # ASSERT
    assert not beekeeper.api.is_wallet_unlocked(wallet_name=wallet.name).unlocked


def test_api_create_temporary_wallet(beekeeper: Beekeeper) -> None:
    """Test test_api_create_temporary_wallet will test beekeeper_api.create of a wallet kept only in memory."""
    # ARRANGE
    name = generate_wallet_name()

    # ACT
    beekeeper.api.create(wallet_name=name, password=generate_wallet_password(), is_temporary=True)

    # ASSERT
    assert beekeeper.api.has_wallet(wallet_name=name).exists
    assert not (beekeeper.settings.ensured_working_directory / f"{name}.wallet").exists()
