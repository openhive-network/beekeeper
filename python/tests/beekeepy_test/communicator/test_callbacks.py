from __future__ import annotations

from dataclasses import replace
from typing import TYPE_CHECKING, Final, NoReturn

import pytest
from beekeepy.communication import (
    AsyncCallbacks,
    Callbacks,
    CommunicationSettings,
    RequestTimings,
    get_communicator_cls,
)
from beekeepy.exceptions import CommunicationError
from beekeepy.interfaces import HttpUrl
from local_tools.beekeepy.network import get_port
from local_tools.beekeepy.testing_server import run_simple_server

if TYPE_CHECKING:
    from beekeepy.communication import ErrorCallback, Request, RequestCallback, RequestContext, ResponseCallback

REQUEST: Final[str] = """{"method": "aaa", "id": 1, "jsonrpc": "2.0"}"""
RESPONSE: Final[str] = """{"jsonrpc": "2.0", "result": {}, "id": 1}"""


class Recorder:
    """Callbacks of all stages, which record their calls."""

    def __init__(self) -> None:
        self.calls: list[str] = []

    def prepare_request(self, name: str) -> RequestCallback:
        def callback(request: Request, /) -> Request:
            self.calls.append(name)
            return replace(request, headers={**request.headers, name: name})

        return callback

    def process_response(self, name: str) -> ResponseCallback:
        def callback(context: RequestContext, /) -> None:
            headers = ",".join(sorted(key for key in context.request.headers if key.startswith("p")))
            self.calls.append(f"{name}:{headers}")
            assert context.response is not None
            assert context.response.body == RESPONSE

        return callback

    def error(self, name: str) -> ErrorCallback:
        def callback(context: RequestContext, /) -> None:
            received = "response" if context.response is not None else "no response"
            self.calls.append(f"{name}:{type(context.exception).__name__}:{received}")

        return callback


def raise_error(_: object, /) -> NoReturn:
    raise RuntimeError("raised by a callback")


def unreachable_url() -> HttpUrl:
    return HttpUrl(f"127.0.0.1:{get_port()}")


def test_callbacks_are_called_in_order_and_requests_are_passed_along() -> None:
    # ARRANGE
    recorder = Recorder()
    communicator = get_communicator_cls("sync")(settings=CommunicationSettings())
    callbacks = Callbacks(
        prepare_request=[recorder.prepare_request("p1"), recorder.prepare_request("p2")],
        process_response=[recorder.process_response("r1"), recorder.process_response("r2")],
    )

    # ACT
    with run_simple_server(RESPONSE) as url:
        response = communicator.post(url=url, data=REQUEST, callbacks=callbacks)

    # ASSERT
    assert response == RESPONSE
    assert recorder.calls == ["p1", "p2", "r1:p1,p2", "r2:p1,p2"]


@pytest.mark.parametrize(
    ("stage", "expected_calls"),
    [
        ("prepare_request", ["e1:RuntimeError:no response", "e2:RuntimeError:no response"]),
        ("process_response", ["e1:RuntimeError:response", "e2:RuntimeError:response"]),
        ("send", ["e1:CommunicationError:no response", "e2:CommunicationError:no response"]),
    ],
)
def test_error_callbacks_are_called_and_the_error_is_raised(stage: str, expected_calls: list[str]) -> None:
    # ARRANGE
    recorder = Recorder()
    communicator = get_communicator_cls("sync")(settings=CommunicationSettings())
    error_callbacks = [recorder.error("e1"), recorder.error("e2")]
    callbacks = {
        "prepare_request": Callbacks(prepare_request=[raise_error], error=error_callbacks),
        "process_response": Callbacks(process_response=[raise_error], error=error_callbacks),
        "send": Callbacks(error=error_callbacks),
    }[stage]

    # ACT & ASSERT
    with run_simple_server(RESPONSE) as url, pytest.raises(CommunicationError if stage == "send" else RuntimeError):
        communicator.post(url=unreachable_url() if stage == "send" else url, data=REQUEST, callbacks=callbacks)
    assert recorder.calls == expected_calls


class TranslatedError(Exception):
    """Error raised by a callback of errors instead of the error of the request."""


def translate_error(context: RequestContext, /) -> NoReturn:
    raise TranslatedError from context.exception


def test_error_raised_by_callback_of_errors_replaces_the_error_and_next_callbacks_are_not_called() -> None:
    # ARRANGE
    recorder = Recorder()
    communicator = get_communicator_cls("sync")(settings=CommunicationSettings())
    callbacks = Callbacks(
        process_response=[raise_error], error=[recorder.error("e1"), translate_error, recorder.error("e2")]
    )

    # ACT
    with run_simple_server(RESPONSE) as url, pytest.raises(TranslatedError) as error:
        communicator.post(url=url, data=REQUEST, callbacks=callbacks)

    # ASSERT
    assert isinstance(error.value.__cause__, RuntimeError)
    assert recorder.calls == ["e1:RuntimeError:response"]


async def test_error_raised_by_callback_of_errors_of_async_communicator_replaces_the_error() -> None:
    # ARRANGE
    recorder = Recorder()
    communicator = get_communicator_cls("async")(settings=CommunicationSettings())
    callbacks = AsyncCallbacks(
        process_response=[raise_error], error=[recorder.error("e1"), translate_error, recorder.error("e2")]
    )

    # ACT
    try:
        with run_simple_server(RESPONSE) as url, pytest.raises(TranslatedError) as error:
            await communicator.async_post(url=url, data=REQUEST, callbacks=callbacks)
    finally:
        await communicator.async_teardown()

    # ASSERT
    assert isinstance(error.value.__cause__, RuntimeError)
    assert recorder.calls == ["e1:RuntimeError:response"]


def test_timings_of_attempts_are_given_to_callbacks() -> None:
    # ARRANGE
    communicator = get_communicator_cls("sync")(settings=CommunicationSettings())
    timings = RequestTimings()
    attempts: list[int] = []

    def record(context: RequestContext, /) -> None:
        assert context.timings is timings
        attempts.append(context.attempt)

    # ACT
    with run_simple_server(RESPONSE) as url:
        for _ in range(2):
            communicator.send(
                url=url, method="POST", data=REQUEST, callbacks=Callbacks(process_response=[record]), timings=timings
            )

    # ASSERT
    assert attempts == [1, 2]
    assert len(timings.attempts) == 2  # noqa: PLR2004
    assert all(duration > 0 for duration in timings.attempts)


class AsyncObjectCallback:
    """Callback object with asynchronous `__call__`."""

    def __init__(self, calls: list[str], name: str) -> None:
        self.calls = calls
        self.name = name

    async def __call__(self, _: RequestContext, /) -> None:
        self.calls.append(self.name)


async def test_async_communicator_awaits_asynchronous_callbacks() -> None:
    # ARRANGE
    calls: list[str] = []

    def sync_callback(_: RequestContext, /) -> None:
        calls.append("sync")

    async def async_function(_: RequestContext, /) -> None:
        calls.append("async function")

    communicator = get_communicator_cls("async")(settings=CommunicationSettings())
    callbacks = AsyncCallbacks(
        process_response=[sync_callback, async_function, AsyncObjectCallback(calls, "async object")],
        error=[AsyncObjectCallback(calls, "error")],
    )

    # ACT
    try:
        with run_simple_server(RESPONSE) as url:
            response = await communicator.async_post(url=url, data=REQUEST, callbacks=callbacks)
        with pytest.raises(CommunicationError):
            await communicator.async_post(url=unreachable_url(), data=REQUEST, callbacks=callbacks)
    finally:
        await communicator.async_teardown()

    # ASSERT
    assert response == RESPONSE
    assert calls == ["sync", "async function", "async object", "error"]
