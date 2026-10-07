from __future__ import annotations

import hiveio_api.app_status_api.app_status_api_description as app_status_api  # noqa: TCH002  # resolved at runtime by get_type_hints
from hiveio_api._apis.abc.api import AbstractAsyncApi


class AppStatusApi(AbstractAsyncApi):
    api = AbstractAsyncApi.endpoint_jsonrpc

    @api
    async def get_app_status(self) -> app_status_api.AppStatusGetAppStatusResponse:
        raise NotImplementedError
