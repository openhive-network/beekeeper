from __future__ import annotations

import json
from typing import TYPE_CHECKING, Any

import pytest
from beekeepy.communication import CommunicationSettings, get_communicator_cls

if TYPE_CHECKING:
    from pathlib import Path

    from beekeepy.handle.runnable import Beekeeper
    from local_tools.beekeepy.models import WalletInfo


def call(beekeeper: Beekeeper, method: str, **params: str) -> dict[str, Any]:
    """Send a beekeeper_api call as raw JSON, with NUL characters as raw bytes rather than JSON escapes."""
    request = json.dumps({"jsonrpc": "2.0", "id": 0, "method": f"beekeeper_api.{method}", "params": params})
    communicator = get_communicator_cls("sync")(settings=CommunicationSettings())
    try:
        response = communicator.post(
            url=beekeeper.settings.ensured_http_endpoint, data=request.replace("\\u0000", "\0")
        )
    finally:
        communicator.teardown()
    result: dict[str, Any] = json.loads(response, strict=False)
    return result


def error_message(response: dict[str, Any]) -> str:
    return str(response.get("error", {}).get("message", ""))


def exists(response: dict[str, Any]) -> object:
    return response.get("result", {}).get("exists", error_message(response))


def move_out_of_wallet_directory(beekeeper: Beekeeper, wallet: WalletInfo) -> Path:
    wallet_dir = beekeeper.settings.ensured_working_directory
    outside_dir = wallet_dir.parent / f"{wallet_dir.name}-outside"
    outside_dir.mkdir()
    beekeeper.api.close(wallet_name=wallet.name)
    return (wallet_dir / f"{wallet.name}.wallet").rename(outside_dir / f"{wallet.name}.wallet")


def name_of(path: Path, kind: str) -> str:
    if kind == "relative":
        return f"../{path.parent.name}/{path.stem}"
    if kind == "absolute":
        return str(path.with_suffix(""))
    return f"../{path.parent.name}/{path.name}\0"


@pytest.mark.parametrize("kind", ["relative", "absolute", "nul"])
def test_api_wallet_name_with_path(beekeeper: Beekeeper, wallet: WalletInfo, kind: str) -> None:
    """Test that has_wallet, open and unlock refuse a wallet name with a path, as create does."""
    # ARRANGE
    outside = move_out_of_wallet_directory(beekeeper, wallet)
    content = outside.read_bytes()
    name = name_of(outside, kind)
    token = beekeeper.api.create_session(salt="names").token

    # ACT
    found = call(beekeeper, "has_wallet", token=token, wallet_name=name)
    opened = call(beekeeper, "open", token=token, wallet_name=name)
    unlocked = call(beekeeper, "unlock", token=token, wallet_name=name, password=wallet.password)

    # ASSERT
    assert exists(found) is False, "A name with a path should not name a wallet."
    assert "Name of wallet is incorrect." in error_message(opened), "Open should refuse a name with a path."
    assert "Name of wallet is incorrect." in error_message(unlocked), "Unlock should refuse a name with a path."
    assert beekeeper.api.list_wallets(token=token).wallets == [], "No wallet should be opened."
    assert outside.read_bytes() == content, "The wallet file should not be changed."

    # The same file in the wallet directory opens as usual.
    outside.rename(beekeeper.settings.ensured_working_directory / outside.name)
    assert exists(call(beekeeper, "has_wallet", token=token, wallet_name=wallet.name)) is True
    unlocked = call(beekeeper, "unlock", token=token, wallet_name=wallet.name, password=wallet.password)
    assert error_message(unlocked) == "", "Unlock of the wallet in the wallet directory should succeed."
