from __future__ import annotations

from dataclasses import dataclass, replace
from datetime import timedelta
from typing import TYPE_CHECKING, Final

import pytest
from beekeepy.communication import ErrorInResponseRules, JsonRpcRules
from beekeepy.exceptions import (
    DifferenceBetweenAmountOfRequestsAndResponsesError,
    ErrorInResponseError,
    GroupedErrorsError,
    OverseerError,
)
from beekeepy.handle.remote import (
    AbstractAsyncApi,
    AbstractAsyncApiCollection,
    AbstractAsyncHandle,
    AbstractSyncApi,
    AbstractSyncApiCollection,
    AbstractSyncHandle,
    AsyncBatchHandle,
    AsyncSendable,
    RemoteHandleSettings,
    SyncBatchHandle,
    SyncSendable,
)
from hiveio_api import HiveModel
from local_tools.beekeepy.testing_server import run_simple_server

if TYPE_CHECKING:
    from beekeepy.communication import (
        AsyncCallbacks,
        Callbacks,
        ErrorCallback,
        Request,
        RequestCallback,
        RequestContext,
        ResponseCallback,
    )
    from beekeepy.interfaces import HttpUrl

RESPONSES: Final[str] = (
    """[{"jsonrpc": "2.0", "result": {"value": 1}, "id": 0}, {"jsonrpc": "2.0", "result": {"value": 2}, "id": 1}]"""
)
RESPONSES_WITH_ERROR: Final[str] = (
    """[{"jsonrpc": "2.0", "result": {"value": 1}, "id": 0},"""
    """ {"jsonrpc": "2.0", "error": {"code": -32000, "message": "Some error"}, "id": 1}]"""
)
OWNERS: Final[list[str]] = ["handle", "batch", "batch-api", "batch-endpoint"]
"""Owners of callbacks of a request of a batch - in the order of their calls."""


class Recorder:
    """Callbacks of all stages, which record their calls."""

    def __init__(self) -> None:
        self.calls: list[str] = []

    def prepare_request(self, name: str) -> RequestCallback:
        def callback(request: Request, /) -> Request:
            self.calls.append(f"prepare:{name}")
            return replace(request, headers={**request.headers, name: name})

        return callback

    def process_response(self, name: str) -> ResponseCallback:
        def callback(context: RequestContext, /) -> None:
            assert name in context.request.headers, "the prepared request is sent"
            self.calls.append(f"process:{name}")

        return callback

    def error(self, name: str) -> ErrorCallback:
        def callback(context: RequestContext, /) -> None:
            self.calls.append(f"error:{name}:{type(context.exception).__name__}")

        return callback

    def register(self, callbacks: Callbacks | AsyncCallbacks, name: str) -> None:
        callbacks.prepare_request.append(self.prepare_request(name))
        callbacks.process_response.append(self.process_response(name))
        callbacks.error.append(self.error(name))


def settings(url: HttpUrl) -> RemoteHandleSettings:
    return RemoteHandleSettings(http_endpoint=url, max_retries=0, period_between_retries=timedelta(0))


@dataclass(kw_only=True)
class Number(HiveModel):
    value: int


class SyncNumberApi(AbstractSyncApi):
    api = AbstractSyncApi.endpoint_jsonrpc

    @classmethod
    def _register_api(cls) -> bool:
        return False

    @api
    def number(self) -> Number:
        raise NotImplementedError


class SyncNumberCollection(AbstractSyncApiCollection):
    def __init__(self, owner: SyncSendable) -> None:
        super().__init__(owner)
        self.number = SyncNumberApi(owner=self._owner)


class SyncNumberHandle(AbstractSyncHandle[RemoteHandleSettings, SyncNumberCollection]):
    def _construct_api(self) -> SyncNumberCollection:
        return self._with_rules(SyncNumberCollection(owner=self))

    def _rules(self) -> list[ResponseCallback]:
        return [JsonRpcRules(), ErrorInResponseRules()]

    def _target_service(self) -> str:
        return "number"

    def batch(self, *, delay_error_on_data_access: bool = False) -> SyncBatchHandle[SyncNumberCollection]:
        return SyncBatchHandle(
            url=self.http_endpoint,
            owner=self,
            api=lambda owner: SyncNumberCollection(owner=owner),
            delay_error_on_data_access=delay_error_on_data_access,
        )


class AsyncNumberApi(AbstractAsyncApi):
    api = AbstractAsyncApi.endpoint_jsonrpc

    @classmethod
    def _register_api(cls) -> bool:
        return False

    @api
    async def number(self) -> Number:
        raise NotImplementedError


class AsyncNumberCollection(AbstractAsyncApiCollection):
    def __init__(self, owner: AsyncSendable) -> None:
        super().__init__(owner)
        self.number = AsyncNumberApi(owner=self._owner)


class AsyncNumberHandle(AbstractAsyncHandle[RemoteHandleSettings, AsyncNumberCollection]):
    def _construct_api(self) -> AsyncNumberCollection:
        return self._with_rules(AsyncNumberCollection(owner=self))

    def _rules(self) -> list[ResponseCallback]:
        return [JsonRpcRules(), ErrorInResponseRules()]

    def _target_service(self) -> str:
        return "number"

    async def batch(self, *, delay_error_on_data_access: bool = False) -> AsyncBatchHandle[AsyncNumberCollection]:
        return AsyncBatchHandle(
            url=self.http_endpoint,
            owner=self,
            api=lambda owner: AsyncNumberCollection(owner=owner),
            delay_error_on_data_access=delay_error_on_data_access,
        )


def register_callbacks(
    recorder: Recorder,
    handle: SyncNumberCollection | AsyncNumberCollection,
    batch: SyncNumberCollection | AsyncNumberCollection,
) -> None:
    recorder.register(handle.callbacks, "handle")
    recorder.register(batch.callbacks, "batch")
    recorder.register(batch.number.callbacks, "batch-api")
    recorder.register(batch.number.number.callbacks, "batch-endpoint")


def test_batch_is_sent_with_callbacks_of_handle_and_of_calls_of_batch() -> None:
    # ARRANGE
    recorder = Recorder()

    # ACT
    with run_simple_server(RESPONSES) as url:
        handle = SyncNumberHandle(settings=settings(url))
        try:
            with handle.batch() as batch:
                register_callbacks(recorder, handle.apis, batch.api)
                first = batch.api.number.number()
                second = batch.api.number.number()
        finally:
            handle.teardown()

    # ASSERT
    assert (first.value, second.value) == (1, 2)
    assert recorder.calls == [f"prepare:{owner}" for owner in OWNERS] + [f"process:{owner}" for owner in OWNERS]


async def test_batch_of_async_handle_is_sent_with_callbacks_of_handle_and_of_calls_of_batch() -> None:
    # ARRANGE
    recorder = Recorder()

    # ACT
    with run_simple_server(RESPONSES) as url:
        handle = AsyncNumberHandle(settings=settings(url))
        try:
            async with await handle.batch() as batch:
                register_callbacks(recorder, handle.apis, batch.api)
                first = await batch.api.number.number()
                second = await batch.api.number.number()
        finally:
            await handle.async_teardown()

    # ASSERT
    assert (first.value, second.value) == (1, 2)
    assert recorder.calls == [f"prepare:{owner}" for owner in OWNERS] + [f"process:{owner}" for owner in OWNERS]


def test_errors_of_batch_are_detected_by_rules_and_given_to_callbacks_of_errors() -> None:
    # ARRANGE
    recorder = Recorder()

    # ACT
    with run_simple_server(RESPONSES_WITH_ERROR) as url:
        handle = SyncNumberHandle(settings=settings(url))
        try:
            with pytest.raises(ErrorInResponseError) as error, handle.batch() as batch:  # noqa: PT012  # sent on exit
                register_callbacks(recorder, handle.apis, batch.api)
                batch.api.number.number()
                batch.api.number.number()
        finally:
            handle.teardown()

    # ASSERT
    assert error.value.request_id == 1
    assert [call for call in recorder.calls if call.startswith("error:")] == [
        f"error:{owner}:Abort" for owner in OWNERS
    ]


async def test_errors_of_batch_of_async_handle_are_detected_by_rules_and_given_to_callbacks_of_errors() -> None:
    # ARRANGE
    recorder = Recorder()

    # ACT
    with run_simple_server(RESPONSES_WITH_ERROR) as url:
        handle = AsyncNumberHandle(settings=settings(url))
        try:
            with pytest.raises(ErrorInResponseError) as error:  # noqa: PT012  # the batch is sent when its context is left
                async with await handle.batch() as batch:
                    register_callbacks(recorder, handle.apis, batch.api)
                    await batch.api.number.number()
                    await batch.api.number.number()
        finally:
            await handle.async_teardown()

    # ASSERT
    assert error.value.request_id == 1
    assert [call for call in recorder.calls if call.startswith("error:")] == [
        f"error:{owner}:Abort" for owner in OWNERS
    ]


class TranslatedError(Exception):
    """Error of a request of a batch translated by the batch handle."""


class TranslatingBatchHandle(SyncBatchHandle[SyncNumberCollection]):
    def _error_of_request(self, error: OverseerError) -> BaseException:
        return TranslatedError(str(error))


@pytest.mark.parametrize("delay_error_on_data_access", [False, True], ids=["raised", "delayed"])
def test_errors_of_requests_of_batch_can_be_translated(delay_error_on_data_access: bool) -> None:
    # ACT
    with run_simple_server(RESPONSES_WITH_ERROR) as url:
        handle = SyncNumberHandle(settings=settings(url))
        try:
            batch_handle = TranslatingBatchHandle(
                url=handle.http_endpoint,
                owner=handle,
                api=lambda owner: SyncNumberCollection(owner=owner),
                delay_error_on_data_access=delay_error_on_data_access,
            )
            with pytest.raises(TranslatedError) as error:  # noqa: PT012  # sent on exit or raised on access
                with batch_handle as batch:
                    first = batch.api.number.number()
                    second = batch.api.number.number()
                assert first.value == 1
                _ = second.value
        finally:
            handle.teardown()

    # ASSERT
    assert "Some error" in str(error.value)


RESPONSES_WITH_ERRORS: Final[str] = (
    """[{"jsonrpc": "2.0", "error": {"code": -32000, "message": "First error"}, "id": 0},"""
    """ {"jsonrpc": "2.0", "error": {"code": -32000, "message": "Second error"}, "id": 1}]"""
)


def test_errors_of_all_requests_of_batch_are_raised_when_their_results_are_read() -> None:
    # ACT
    with run_simple_server(RESPONSES_WITH_ERRORS) as url:
        handle = SyncNumberHandle(settings=settings(url))
        try:
            with handle.batch(delay_error_on_data_access=True) as batch:
                first = batch.api.number.number()
                second = batch.api.number.number()
        finally:
            handle.teardown()

    # ASSERT - errors of all responses are given by the rule (grouped), each one to its request
    with pytest.raises(ErrorInResponseError) as first_error:
        _ = first.value
    with pytest.raises(ErrorInResponseError) as second_error:
        _ = second.value
    assert (first_error.value.request_id, second_error.value.request_id) == (0, 1)
    assert "First error" in str(first_error.value)
    assert "Second error" in str(second_error.value)


MAX_RETRIES: Final[int] = 2


def retried_settings(url: HttpUrl) -> RemoteHandleSettings:
    return RemoteHandleSettings(http_endpoint=url, max_retries=MAX_RETRIES, period_between_retries=timedelta(0))


def test_whole_batch_is_sent_again_while_its_responses_are_retried() -> None:
    # ARRANGE - three requests, two responses
    sent: list[str] = []

    def record(request: Request, /) -> Request:
        sent.append(request.body or "")
        return request

    # ACT
    with run_simple_server(RESPONSES) as url:
        handle = SyncNumberHandle(settings=retried_settings(url))
        handle.apis.callbacks.prepare_request.append(record)
        try:
            with pytest.raises(DifferenceBetweenAmountOfRequestsAndResponsesError) as error, handle.batch() as batch:  # noqa: PT012  # sent on exit
                for _ in range(3):
                    batch.api.number.number()
        finally:
            handle.teardown()

    # ASSERT
    assert len(sent) == 1 + MAX_RETRIES
    assert len(set(sent)) == 1, "the same batch is sent each time"
    assert isinstance(error.value.cause, GroupedErrorsError)


async def test_whole_batch_of_async_handle_is_sent_again_while_its_responses_are_retried() -> None:
    # ARRANGE - three requests, two responses
    sent: list[str] = []

    def record(request: Request, /) -> Request:
        sent.append(request.body or "")
        return request

    # ACT
    with run_simple_server(RESPONSES) as url:
        handle = AsyncNumberHandle(settings=retried_settings(url))
        handle.apis.callbacks.prepare_request.append(record)
        try:
            with pytest.raises(DifferenceBetweenAmountOfRequestsAndResponsesError):  # noqa: PT012  # sent on exit
                async with await handle.batch() as batch:
                    for _ in range(3):
                        await batch.api.number.number()
        finally:
            await handle.async_teardown()

    # ASSERT
    assert len(sent) == 1 + MAX_RETRIES
    assert len(set(sent)) == 1, "the same batch is sent each time"
