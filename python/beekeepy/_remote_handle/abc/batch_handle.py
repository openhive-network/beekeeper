from __future__ import annotations

from abc import ABC
from collections.abc import Callable
from dataclasses import dataclass
from typing import TYPE_CHECKING, Any, Generic, Protocol, TypeVar

from hiveio_api.client import (
    AsyncCallbacks,
    AsyncSendable,
    Callbacks,
    ExpectResultT,
    JSONRPCResult,
    SyncSendable,
    build_response,
)

from beekeepy import exceptions
from beekeepy._utilities.context import ContextAsync, ContextSync, EnterReturnT

if TYPE_CHECKING:
    from types import TracebackType
    from typing import Self

    from hiveio_api.client import HttpUrl, Methods


class SyncBatchOwner(Protocol):
    """Handle sending requests of its batches - with callbacks of its API collection, then the given ones."""

    def _send_batch(
        self, *, url: HttpUrl, data: str, callbacks: Callbacks
    ) -> exceptions.Json | list[exceptions.Json]: ...


class AsyncBatchOwner(Protocol):
    """Handle sending requests of its batches - with callbacks of its API collection, then the given ones."""

    async def _async_send_batch(
        self, *, url: HttpUrl, data: str, callbacks: AsyncCallbacks
    ) -> exceptions.Json | list[exceptions.Json]: ...


class _DelayedResponseWrapper:
    def __init__(self, url: HttpUrl, request: str, expected_type: type[ExpectResultT]) -> None:
        super().__setattr__("_url", url)
        super().__setattr__("_request", request)
        super().__setattr__("_response", None)
        super().__setattr__("_exception", None)
        super().__setattr__("_expected_type", expected_type)

    def __check_is_response_available(self) -> None:
        if (exception := super().__getattribute__("_exception")) is not None:
            raise exception
        if self.__get_data() is None:
            raise exceptions.ResponseNotReadyError

    def __get_data(self) -> Any:
        response = super().__getattribute__("_response")
        if response is None:
            return None

        if isinstance(response, JSONRPCResult):
            return response.result
        return response

    def __setattr__(self, name: str, value: Any, /) -> None:
        self.__check_is_response_available()
        setattr(self.__get_data(), name, value)

    def __getattr__(self, name: str, /) -> Any:
        self.__check_is_response_available()
        return getattr(self.__get_data(), name)

    def _set_response(self, **kwargs: Any) -> None:
        expected_type = super().__getattribute__("_expected_type")
        response = build_response(expected_type, kwargs)
        assert isinstance(response, JSONRPCResult), "Expected JSONRPCResult, model cannot be found."
        super().__setattr__("_response", response.result)

    def _set_exception(self, exception: BaseException) -> None:
        super().__setattr__("_exception", exception)


@dataclass(kw_only=True)
class _BatchRequestResponseItem:
    request: str
    delayed_result: _DelayedResponseWrapper


class _PostRequestManager(ContextSync["_PostRequestManager"]):
    def __init__(self, owner: _BatchHandle[Any]) -> None:
        self.__owner = owner
        self.__responses: list[dict[str, Any]] = []

    def set_responses(self, responses: exceptions.Json | list[exceptions.Json]) -> None:
        assert isinstance(responses, list), f"Expected list of dictionaries, got: {responses=}"
        self.__responses = responses

    def _enter(self) -> _PostRequestManager:
        return self

    def __set_response_or_exception(
        self, request_id: int, response: exceptions.OverseerError | exceptions.Json
    ) -> None:
        if isinstance(response, exceptions.OverseerError):
            error = self.__owner._error_of_request(response)
            self.__owner._get_batch_delayed_result(request_id)._set_exception(error)
            if not self.__owner._delay_error_on_data_access:
                raise error
        else:
            self.__owner._get_batch_delayed_result(request_id)._set_response(**response)

    def __handle_no_exception_case(self) -> None:
        for response in self.__responses:
            self.__set_response_or_exception(request_id=int(response["id"]), response=response)

    def __handle_exception_case(self, exception: BaseException) -> bool:
        if (not isinstance(exception, exceptions.OverseerError)) or isinstance(
            exception,
            (
                exceptions.DifferenceBetweenAmountOfRequestsAndResponsesError,
                exceptions.JussiResponseError,
                exceptions.UnparsableResponseError,
            ),
        ):
            return False

        assert isinstance(exception.whole_response, list), (
            f"response has to be list, but is: `{type(exception.response)}`, content: '{exception.response=}'"
        )
        all_exceptions = exception.cause
        assert all_exceptions is not None, "Cause is set to None"
        assert isinstance(all_exceptions, exceptions.GroupedErrorsError), (
            f"Cause (after from clause) have to be GroupedErrorsError, but is: {type(all_exceptions)}"
        )

        for response in exception.whole_response:
            request_id = response["id"]
            specific_exception = all_exceptions.get_exception_for(request_id=request_id)
            self.__set_response_or_exception(request_id=int(request_id), response=specific_exception or response)
        return self.__owner._delay_error_on_data_access

    def _handle_exception(self, exception: BaseException, __: TracebackType | None) -> bool:
        return self.__handle_exception_case(exception)

    def _handle_no_exception(self) -> None:
        self.__handle_no_exception_case()

    def _finally(self) -> None:
        return None


class _BatchHandle(ContextSync[EnterReturnT], ContextAsync[EnterReturnT], ABC, Generic[EnterReturnT]):
    """
    Batch of requests - sent as one request when the context is left.

    The request of the batch is sent with callbacks of the API collection of the owner (with its rules), then with
    callbacks of all calls of the batch (of the API collection of the batch, its APIs and endpoints) - each callback
    once, as for a single request.
    """

    def __init__(
        self,
        url: HttpUrl,
        *args: Any,
        delay_error_on_data_access: bool = False,
        is_testnet: bool = False,
        **kwargs: Any,
    ) -> None:
        super().__init__(*args, **kwargs)
        self._url = url
        self._delay_error_on_data_access = delay_error_on_data_access

        self.__batch: list[_BatchRequestResponseItem] = []

        self.__is_testnet: bool = is_testnet

    def _next_id_for_request(self) -> int:
        """Returns next id for request."""
        return len(self.__batch)

    def _error_of_request(self, error: exceptions.OverseerError) -> BaseException:
        """
        Exception of a request of the batch, given the error detected for it - raised or kept until its result is read.

        The error itself by default; a subclass may translate it (e.g. to exceptions of a library using the handle).
        """
        return error

    def _impl_handle_request(self, data: str, *, expect_type: type[ExpectResultT]) -> ExpectResultT:
        @dataclass
        class DummyResponse:
            result: Any

        delayed_result = _DelayedResponseWrapper(url=self._url, request=data, expected_type=expect_type)
        self.__batch.append(_BatchRequestResponseItem(request=data, delayed_result=delayed_result))
        return DummyResponse(result=delayed_result)  # type: ignore[return-value]

    def __sync_evaluate(self) -> None:
        query = self.__prepare_request()

        with _PostRequestManager(self) as mgr:
            mgr.set_responses(self._send_query(query))

    async def __async_evaluate(self) -> None:
        query = self.__prepare_request()

        with _PostRequestManager(self) as mgr:
            mgr.set_responses(await self._async_send_query(query))

    def _send_query(self, query: str) -> exceptions.Json | list[exceptions.Json]:  # noqa: ARG002
        raise AssertionError("Synchronous batch requires synchronous handle")

    async def _async_send_query(self, query: str) -> exceptions.Json | list[exceptions.Json]:  # noqa: ARG002
        raise AssertionError("Asynchronous batch requires asynchronous handle")

    def __prepare_request(self) -> str:
        return "[" + ",".join([x.request for x in self.__batch]) + "]"

    def _get_batch_delayed_result(self, request_id: int) -> _DelayedResponseWrapper:
        return self.__batch[request_id].delayed_result

    def __is_anything_to_send(self) -> bool:
        return bool(self.__batch)

    async def _aenter(self) -> EnterReturnT:
        return self  # type: ignore[return-value]

    def _enter(self) -> EnterReturnT:
        return self  # type: ignore[return-value]

    async def _ahandle_no_exception(self) -> None:
        if not self.__is_anything_to_send():
            raise exceptions.NothingToSendError

        await self.__async_evaluate()

    def _handle_no_exception(self) -> None:
        if not self.__is_anything_to_send():
            raise exceptions.NothingToSendError

        self.__sync_evaluate()

    async def _afinally(self) -> None:
        return None

    def _finally(self) -> None:
        return None

    def is_testnet(self) -> bool:
        return self.__is_testnet


ApiT = TypeVar("ApiT")
OwnerT = TypeVar("OwnerT")


ApiFactory = Callable[[OwnerT], ApiT]


class SyncBatchHandle(_BatchHandle["SyncBatchHandle"], SyncSendable, Generic[ApiT]):  # type: ignore[type-arg]
    def __init__(
        self,
        url: HttpUrl,
        owner: SyncBatchOwner,
        api: ApiFactory[Self, ApiT],
        *args: Any,
        delay_error_on_data_access: bool = False,
        is_testnet: bool = False,
        **kwargs: Any,
    ) -> None:
        super().__init__(
            url, *args, delay_error_on_data_access=delay_error_on_data_access, is_testnet=is_testnet, **kwargs
        )
        self.__owner = owner
        self.__callbacks: list[Callbacks] = []
        """Callbacks of calls of the batch."""
        self.api: ApiT = api(self)

    def _send(
        self,
        *,
        method: Methods,  # noqa: ARG002
        expected_type: type[ExpectResultT],
        data: str | None = None,
        url: HttpUrl | None = None,  # noqa: ARG002
        callbacks: Callbacks | None = None,
    ) -> JSONRPCResult[ExpectResultT]:
        if callbacks is not None:
            self.__callbacks.append(callbacks)
        return self._impl_handle_request(data, expect_type=expected_type)  # type: ignore[arg-type]

    def _send_query(self, query: str) -> exceptions.Json | list[exceptions.Json]:
        return self.__owner._send_batch(url=self._url, data=query, callbacks=Callbacks.joined(*self.__callbacks))

    def _id_for_jsonrpc_request(self) -> int:
        return self._next_id_for_request()


class AsyncBatchHandle(_BatchHandle["AsyncBatchHandle"], AsyncSendable, Generic[ApiT]):  # type: ignore[type-arg]
    def __init__(
        self,
        url: HttpUrl,
        owner: AsyncBatchOwner,
        api: ApiFactory[Self, ApiT],
        *args: Any,
        delay_error_on_data_access: bool = False,
        is_testnet: bool = False,
        **kwargs: Any,
    ) -> None:
        super().__init__(
            url, *args, delay_error_on_data_access=delay_error_on_data_access, is_testnet=is_testnet, **kwargs
        )
        self.__owner = owner
        self.__callbacks: list[AsyncCallbacks] = []
        """Callbacks of calls of the batch."""
        self.api: ApiT = api(self)

    async def _async_send(
        self,
        *,
        method: Methods,  # noqa: ARG002
        expected_type: type[ExpectResultT],
        data: str | None = None,
        url: HttpUrl | None = None,  # noqa: ARG002
        callbacks: AsyncCallbacks | None = None,
    ) -> JSONRPCResult[ExpectResultT]:
        if callbacks is not None:
            self.__callbacks.append(callbacks)
        return self._impl_handle_request(data, expect_type=expected_type)  # type: ignore[arg-type]

    async def _async_send_query(self, query: str) -> exceptions.Json | list[exceptions.Json]:
        return await self.__owner._async_send_batch(
            url=self._url, data=query, callbacks=AsyncCallbacks.joined(*self.__callbacks)
        )

    def _id_for_jsonrpc_request(self) -> int:
        return self._next_id_for_request()
