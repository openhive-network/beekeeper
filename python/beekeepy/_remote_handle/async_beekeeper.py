from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy._apis.abc.session_holder import (
    AsyncSessionHolder,
)
from beekeepy._apis.beekeeper_api import (
    AsyncBeekeeperApi,
    BeekeeperAsyncApiCollection,
)
from beekeepy._communication.rules import BeekeeperRules, ErrorInResponseRules, JsonRpcRules
from beekeepy._remote_handle.abc.batch_handle import ApiFactory, AsyncBatchHandle
from beekeepy._remote_handle.abc.handle import AbstractAsyncHandle, RemoteSettingsT
from beekeepy._remote_handle.commons import (
    handle_target_service_name,
    raise_acquire_not_possible,
    random_string,
)
from beekeepy._utilities.sanitize import sanitize

if TYPE_CHECKING:
    from typing import Self

    from hiveio_api.client import HttpUrl, ResponseCallback

    from beekeepy._remote_handle.abc.batch_handle import AsyncBatchOwner
    from beekeepy.exceptions import Json


class _AsyncSessionBatchHandle(AsyncBatchHandle[BeekeeperAsyncApiCollection], AsyncSessionHolder):
    def __init__(  # noqa: PLR0913
        self,
        url: HttpUrl,
        owner: AsyncBatchOwner,
        api: ApiFactory[Self, BeekeeperAsyncApiCollection],
        *args: Any,
        session_token: str,
        delay_error_on_data_access: bool = False,
        is_testnet: bool = False,
        **kwargs: Any,
    ) -> None:
        super().__init__(
            url,
            owner,
            api,
            *args,
            delay_error_on_data_access=delay_error_on_data_access,
            is_testnet=is_testnet,
            **kwargs,
        )
        self.set_session_token(session_token)

    async def _acquire_session_token(self) -> str:
        raise_acquire_not_possible()


class AsyncBeekeeper(AbstractAsyncHandle[RemoteSettingsT, BeekeeperAsyncApiCollection], AsyncSessionHolder):
    """Asynchronous handle for beekeeper service communication."""

    def _construct_api(self) -> BeekeeperAsyncApiCollection:
        return self._with_rules(BeekeeperAsyncApiCollection(owner=self))

    def _rules(self) -> list[ResponseCallback]:
        return [JsonRpcRules(), BeekeeperRules(), ErrorInResponseRules()]

    @property
    def apis(self) -> BeekeeperAsyncApiCollection:
        return super().api

    @property
    def api(self) -> AsyncBeekeeperApi:  # type: ignore[override]
        return self.apis.beekeeper

    def _target_service(self) -> str:
        return handle_target_service_name

    async def batch(self, *, delay_error_on_data_access: bool = False) -> AsyncBatchHandle[BeekeeperAsyncApiCollection]:
        return _AsyncSessionBatchHandle(
            url=self.http_endpoint,
            owner=self,
            api=lambda o: BeekeeperAsyncApiCollection(owner=o),
            delay_error_on_data_access=delay_error_on_data_access,
            session_token=(await self.session).token,
            is_testnet=self.is_testnet(),
        )

    async def _acquire_session_token(self) -> str:
        return (await self.api.create_session(salt=self._get_salt())).token

    def _get_salt(self) -> str:
        return random_string()

    def _sanitize_data(self, data: Json | list[Json] | str) -> Json | list[Json] | str:
        return sanitize(data)


AsyncBeekeeperTemplate = AsyncBeekeeper
