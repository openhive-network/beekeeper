from __future__ import annotations

from datetime import timedelta
from typing import TYPE_CHECKING

from beekeepy._executable.defaults import BeekeeperDefaults

from schemas.fields.hive_datetime import HiveDateTime

if TYPE_CHECKING:
    from beekeepy.handle.runnable import Beekeeper


def test_api_get_info(beekeeper: Beekeeper) -> None:
    """Test test_api_get_info will test beekeeper_api.get_info api call."""
    # ARRANGE
    unlock_timeout = BeekeeperDefaults.DEFAULT_UNLOCK_TIMEOUT
    tolerance_secs = 1

    # ACT
    get_info = beekeeper.api.get_info()
    now = HiveDateTime(get_info.now)
    timeout_time = HiveDateTime(get_info.timeout_time)

    # ASSERT
    upper_bound = now + timedelta(seconds=(unlock_timeout + tolerance_secs))
    lower_bound = now + timedelta(seconds=(unlock_timeout - tolerance_secs))
    message = (
        f"Difference between get_info.now and get_info.timeout_time should be equal {unlock_timeout} (+/-"
        f" {tolerance_secs}s)"
    )
    assert lower_bound <= timeout_time <= upper_bound, message
