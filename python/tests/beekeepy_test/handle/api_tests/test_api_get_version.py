from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from beekeepy.handle.runnable import Beekeeper


def test_api_get_version(beekeeper: Beekeeper) -> None:
    """Test test_api_get_version will test beekeeper_api.get_version api call."""
    # ACT
    version = beekeeper.api.get_version().version

    # ASSERT
    assert int(version, 16), "Version is a git revision (hash)"
