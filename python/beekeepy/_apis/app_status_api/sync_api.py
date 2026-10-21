from __future__ import annotations

import hiveio_api.app_status_api.app_status_api_description as app_status_api  # noqa: TCH002  # resolved at runtime by get_type_hints
from hiveio_api.client import AbstractSyncApi


class AppStatusApi(AbstractSyncApi):
    """Synchronous version of `hiveio_api.app_status_api.AppStatusApi` (the generator creates asynchronous APIs)."""

    __validation_module__ = "hiveio_api._validation.app_status_api"
    api = AbstractSyncApi.endpoint_jsonrpc

    @api
    def get_app_status(self) -> app_status_api.AppStatusGetAppStatusResponse:
        raise NotImplementedError
