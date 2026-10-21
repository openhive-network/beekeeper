from __future__ import annotations

import asyncio
import time
from abc import ABC, abstractmethod
from datetime import UTC, datetime
from typing import TYPE_CHECKING, Any, Generic, TypeVar, cast

from hiveio_api.client import (
    AbstractAsyncApiCollection,
    AbstractSyncApiCollection,
    AsyncCallbacks,
    AsyncSendable,
    Callbacks,
    ExpectResultT,
    HttpUrl,
    JSONRPCResult,
    Request,
    RequestTimings,
    SyncSendable,
    build_response,
)
from loguru import logger as loguru_logger

from beekeepy._communication.communicator_getter import get_communicator_cls
from beekeepy._communication.rules import ErrorInResponseRules, JsonRpcRules
from beekeepy._communication.signals import Abort, Retry
from beekeepy._remote_handle.settings import RemoteHandleSettings
from beekeepy._utilities.context import SelfContextAsync, SelfContextSync
from beekeepy._utilities.settings_holder import UniqueSettingsHolder
from beekeepy._utilities.stopwatch import Stopwatch
from beekeepy.exceptions import CommunicationError, GroupedErrorsError

if TYPE_CHECKING:
    from hiveio_api.client import Methods, Response, ResponseCallback
    from loguru import Logger

    from beekeepy._communication.abc.communicator import AbstractCommunicator
    from beekeepy._remote_handle.abc.batch_handle import AsyncBatchHandle, SyncBatchHandle
    from beekeepy.exceptions import Json

RemoteSettingsT = TypeVar("RemoteSettingsT", bound=RemoteHandleSettings)


ApiT = TypeVar("ApiT", bound=AbstractAsyncApiCollection | AbstractSyncApiCollection)
CollectionT = TypeVar("CollectionT", bound="AbstractSyncApiCollection | AbstractAsyncApiCollection")
CallbacksT = TypeVar("CallbacksT", Callbacks, AsyncCallbacks)
T = TypeVar("T")


class AbstractHandle(UniqueSettingsHolder[RemoteSettingsT], ABC, Generic[RemoteSettingsT, ApiT]):
    """Provides basic interface for all network handles."""

    def __init__(
        self,
        *args: Any,
        settings: RemoteSettingsT,
        logger: Logger | None = None,
        **kwargs: Any,
    ) -> None:
        """Constructs handle to network service.

        Args:
            http_url: http url where, service is available.
            communicator: communicator class to use for communication
        """
        super().__init__(*args, settings=settings, **kwargs)
        self.__logger = self.__configure_logger(logger)
        self.__communicator = self.settings.try_get_communicator_instance() or self._get_recommended_communicator()
        self.__api = self._construct_api()

    @property
    def http_endpoint(self) -> HttpUrl:  # TODO: RESOLVE APPROPRIATE SETTING HANDLE
        """Return endpoint where handle is connected to."""
        assert self.settings.http_endpoint is not None, "Http endpoint shouldn't be None"
        return self.settings.http_endpoint

    @http_endpoint.setter
    def http_endpoint(self, value: HttpUrl) -> None:
        """Set http endpoint."""
        self.logger.debug(f"setting http endpoint to: {value.as_string()}")
        with self.update_settings() as settings:
            settings.http_endpoint = value

    @property
    def apis(self) -> ApiT:
        return self.__api

    @property
    def api(self) -> ApiT:
        return self.__api

    @property
    def _communicator(self) -> AbstractCommunicator:
        """Return communicator. Internal only."""
        return self.__communicator

    @property
    def logger(self) -> Logger:
        return self.__logger

    @abstractmethod
    def _get_recommended_communicator(self) -> AbstractCommunicator:
        """Return api collection."""

    @abstractmethod
    def _construct_api(self) -> ApiT:
        """Return api collection - with rules of the handle (`_with_rules`)."""

    def _rules(self) -> list[ResponseCallback]:
        """
        Rules of responses of the service - callbacks of responses checked before all other ones.

        Override to add rules of the service between rules of JSON-RPC and the rule of any other error.
        """
        return [JsonRpcRules(), ErrorInResponseRules()]

    def _with_rules(self, api: CollectionT) -> CollectionT:
        """
        Add rules of the handle to callbacks of responses of the api collection.

        Requests of the handle - single ones and batches - are checked by callbacks of its api collection.
        """
        api.callbacks.process_response.extend(self._rules())
        return api

    def _retries_left(self, signal: Retry | Abort, retries_left: int, timings: RequestTimings) -> int:
        """
        Retries left after the one requested by the signal - raises its errors when it aborts or no retry is left.

        Unlimited retries do not use limited ones, they are limited only by `unlimited_retries_timeout` of settings.
        """
        if isinstance(signal, Retry):
            if signal.unlimited:
                timeout = self.settings.unlimited_retries_timeout
                if timeout is None or datetime.now(UTC) - timings.started < timeout:
                    return retries_left
            elif retries_left > 0:
                return retries_left - 1
        raise signal.errors[0] from GroupedErrorsError(signal.errors)

    def _with_callbacks_of_apis(self, callbacks: CallbacksT) -> CallbacksT:
        """Callbacks of the api collection of the handle followed by the given ones - each callback once."""
        own_callbacks = self.apis.callbacks
        assert isinstance(own_callbacks, type(callbacks)), "Handles and their batches are both (a)synchronous"
        return _each_once(type(callbacks).joined(own_callbacks, callbacks))

    @abstractmethod
    def _is_synchronous(self) -> bool:
        """Returns is handle is asynchronous."""

    @abstractmethod
    def _target_service(self) -> str:
        """Returns name of service that following handle is connecting to."""

    def _logger_extras(self) -> dict[str, Any]:
        """
        Override to pass additional extras to loguru.logger.bind.

        Learn more: https://loguru.readthedocs.io/en/stable/api/logger.html#loguru._logger.Logger.bind
        """
        return {
            "asynchronous": not self._is_synchronous(),
            "handle_target": self._target_service(),
        }

    @classmethod
    def _response_handle(
        cls,
        response: Json | list[Json],
        expected_type: type[ExpectResultT],
        *,
        is_jsonrpc: bool,
    ) -> JSONRPCResult[ExpectResultT]:
        """Builds response (without validation)."""
        if is_jsonrpc:
            assert isinstance(response, dict), f"Expected dict as response, got: {response=}"
        else:
            response = {"result": response, "jsonrpc": "2.0", "id": 0}
        built = build_response(expected_type, response)
        assert isinstance(built, JSONRPCResult)
        return built

    def __configure_logger(self, logger: Logger | None) -> Logger:
        # credit for lazy=True: https://github.com/Delgan/loguru/issues/402#issuecomment-2028011786
        return (logger or loguru_logger).opt(lazy=True).bind(**self._logger_extras())

    def teardown(self) -> None:
        self._communicator.teardown()

    async def async_teardown(self) -> None:
        await self._communicator.async_teardown()

    def _sanitize_data(self, data: Json | list[Json] | str) -> Json | list[Json] | str:
        return data

    def _log_request(self, url: HttpUrl, request: str | None) -> None:
        self.logger.trace(
            "sending to `{}` data: `{}`",
            url.as_string,
            lambda: self._sanitize_data(request or ""),  # to reduce deepcopy
        )

    def _log_response(self, seconds_delta: float, response: Json | list[Json]) -> None:
        self.logger.trace(
            f"got response in {seconds_delta :.5f}s from " + "`{}`: `{}`",
            self.http_endpoint.as_string,
            lambda: self._sanitize_data(response),  # to reduce deepcopy
        )

    def is_testnet(self) -> bool:
        """Returns if handle is connected to testnet."""
        return False

    def _merge_url(self, query_url: HttpUrl | None) -> HttpUrl:
        """Merges given url with path."""
        return HttpUrl.factory(
            address=self.http_endpoint.address,
            port=self.http_endpoint.port,
            path=(query_url or self.http_endpoint).path,
            query=(query_url or self.http_endpoint).query,
            protocol=self.http_endpoint.protocol,
        )

    @classmethod
    def _is_jsonrpc(cls, url: HttpUrl, method: Methods, data: str | None) -> bool:
        """Whether the request is a JSON-RPC call - recognized as by rules of responses (`Request.jsonrpc`)."""
        return bool(Request(url=url, method=method, headers={}, body=data).jsonrpc)


class AbstractAsyncHandle(AbstractHandle[RemoteSettingsT, ApiT], SelfContextAsync, AsyncSendable, ABC):
    """Base class for service handlers that uses asynchronous communication."""

    async def _async_send(
        self,
        *,
        method: Methods,
        expected_type: type[ExpectResultT],
        data: str | None = None,
        url: HttpUrl | None = None,
        callbacks: AsyncCallbacks | None = None,
    ) -> JSONRPCResult[ExpectResultT]:
        """Sends data asynchronously to handled service basing on jsonrpc."""
        from beekeepy._utilities.error_logger import ErrorLogger

        final_url = self._merge_url(url)
        self._log_request(final_url, data)
        with Stopwatch() as record, ErrorLogger(self.logger, CommunicationError):
            response = _parsed(
                await self._async_send_request(url=final_url, method=method, data=data, callbacks=callbacks)
            )
        self._log_response(record.seconds_delta, response)
        return self._response_handle(
            response=response,
            expected_type=expected_type,
            is_jsonrpc=self._is_jsonrpc(final_url, method, data),
        )

    async def _async_send_request(
        self, *, url: HttpUrl, method: Methods, data: str | None, callbacks: AsyncCallbacks | None
    ) -> Response:
        """Send the request through the communicator - again, while callbacks of its response raise `Retry`."""
        timings = RequestTimings()
        retries_left = self.settings.max_retries
        while True:
            try:  # the loop retries the request
                return await self._communicator.async_send(
                    url=url, method=method, data=data, callbacks=callbacks, timings=timings
                )
            except (Retry, Abort) as signal:  # noqa: PERF203  # the loop retries the request
                retries_left = self._retries_left(signal, retries_left, timings)
                await asyncio.sleep(self.settings.period_between_retries.total_seconds())

    async def _async_send_batch(self, *, url: HttpUrl, data: str, callbacks: AsyncCallbacks) -> Json | list[Json]:
        """Send the request of a batch - with callbacks of the api collection of the handle, then the given ones."""
        response = await self._async_send_request(
            url=url, method="POST", data=data, callbacks=self._with_callbacks_of_apis(callbacks)
        )
        return _parsed(response)

    def _is_synchronous(self) -> bool:
        return False

    def _get_recommended_communicator(self) -> AbstractCommunicator:
        return get_communicator_cls("async")(settings=self._settings)

    @abstractmethod
    async def batch(self, *, delay_error_on_data_access: bool = False) -> AsyncBatchHandle[Any]:
        """Returns async batch handle."""

    async def _afinally(self) -> None:
        await self.async_teardown()


class AbstractSyncHandle(AbstractHandle[RemoteSettingsT, ApiT], SelfContextSync, SyncSendable, ABC):
    """Base class for service handlers that uses synchronous communication."""

    def _send(
        self,
        *,
        method: Methods,
        expected_type: type[ExpectResultT],
        data: str | None = None,
        url: HttpUrl | None = None,
        callbacks: Callbacks | None = None,
    ) -> JSONRPCResult[ExpectResultT]:
        """Sends data synchronously to handled service basing on jsonrpc."""
        from beekeepy._utilities.error_logger import ErrorLogger

        final_url = self._merge_url(url)
        self._log_request(final_url, data)
        with Stopwatch() as record, ErrorLogger(self.logger, CommunicationError):
            response = _parsed(self._send_request(url=final_url, method=method, data=data, callbacks=callbacks))
        self._log_response(record.seconds_delta, response)
        return self._response_handle(
            response=response,
            expected_type=expected_type,
            is_jsonrpc=self._is_jsonrpc(final_url, method, data),
        )

    def _send_request(
        self, *, url: HttpUrl, method: Methods, data: str | None, callbacks: Callbacks | None
    ) -> Response:
        """Send the request through the communicator - again, while callbacks of its response raise `Retry`."""
        timings = RequestTimings()
        retries_left = self.settings.max_retries
        while True:
            try:  # the loop retries the request
                return self._communicator.send(url=url, method=method, data=data, callbacks=callbacks, timings=timings)
            except (Retry, Abort) as signal:  # noqa: PERF203  # the loop retries the request
                retries_left = self._retries_left(signal, retries_left, timings)
                time.sleep(self.settings.period_between_retries.total_seconds())

    def _send_batch(self, *, url: HttpUrl, data: str, callbacks: Callbacks) -> Json | list[Json]:
        """Send the request of a batch - with callbacks of the api collection of the handle, then the given ones."""
        response = self._send_request(
            url=url, method="POST", data=data, callbacks=self._with_callbacks_of_apis(callbacks)
        )
        return _parsed(response)

    def _is_synchronous(self) -> bool:
        return True

    def _get_recommended_communicator(self) -> AbstractCommunicator:
        return get_communicator_cls("sync")(settings=self._settings)

    @abstractmethod
    def batch(self, *, delay_error_on_data_access: bool = False) -> SyncBatchHandle[Any]:
        """Returns sync batch handle."""

    def _finally(self) -> None:
        self.teardown()


def _parsed(response: Response) -> Json | list[Json]:
    # objects of JSON-RPC (arrays of them in a batch) or results of REST APIs - accepted by rules of the handle
    return cast("Json | list[Json]", response.json)


def _each_once(callbacks: CallbacksT) -> CallbacksT:
    """
    Callbacks with each callback once - in the order of its first occurrence.

    Calls of a batch give callbacks of their api collection (and of their APIs) - the same ones for each call.
    """
    return type(callbacks)(
        prepare_request=_unique(callbacks.prepare_request),
        process_response=_unique(callbacks.process_response),
        error=_unique(callbacks.error),
    )


def _unique(items: list[T]) -> list[T]:
    return list({id(item): item for item in items}.values())
