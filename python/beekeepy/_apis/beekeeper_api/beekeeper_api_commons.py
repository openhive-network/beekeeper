from __future__ import annotations

from abc import abstractmethod
from typing import TYPE_CHECKING, Any, Generic

from hiveio_api.client import HandleT

if TYPE_CHECKING:
    from beekeepy._apis.abc.session_holder import AsyncSessionHolder, SyncSessionHolder


class BeekeeperApiCommons(Generic[HandleT]):
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)

    def _token_required(self, endpoint_name: str) -> bool:
        return endpoint_name not in {"beekeeper_api.create_session", "beekeeper_api.get_version"}

    def _verify_is_owner_can_hold_session_token(self, owner: HandleT) -> None:
        assert isinstance(
            owner, self._get_requires_session_holder_type()
        ), f"owner `{owner}` is not able to handle this request"

    @abstractmethod
    def _get_requires_session_holder_type(self) -> type[SyncSessionHolder | AsyncSessionHolder]: ...
