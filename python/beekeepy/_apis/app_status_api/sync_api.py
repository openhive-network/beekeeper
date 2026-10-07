from __future__ import annotations

import hiveio_api.app_status_api.app_status_api_description as app_status_api  # noqa: TCH002  # resolved at runtime by get_type_hints
from hiveio_api._apis.abc.api import AbstractSyncApi


class AppStatusApi(AbstractSyncApi):
    api = AbstractSyncApi.endpoint_jsonrpc

    @api
    def get_app_status(self) -> app_status_api.AppStatusGetAppStatusResponse:
        raise NotImplementedError
