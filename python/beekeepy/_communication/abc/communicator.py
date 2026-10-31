from __future__ import annotations

import inspect
import time
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, TypeVar, cast

from hiveio_api.client import (
    AsyncCallbacks,
    Callbacks,
    Methods,
    Request,
    RequestContext,
    RequestTimings,
    Response,
)

from beekeepy._communication.settings import CommunicationSettings
from beekeepy._utilities.settings_holder import SharedSettingsHolder
from beekeepy.exceptions import TimeoutExceededError

if TYPE_CHECKING:
    from collections.abc import Awaitable

    from hiveio_api.client import HttpUrl


T = TypeVar("T")


def default_callback() -> Callbacks:
    return Callbacks()


def default_async_callback() -> AsyncCallbacks:
    return AsyncCallbacks()


class AbstractCommunicator(SharedSettingsHolder[CommunicationSettings], ABC):
    """Provides basic interface for communicators, which can implement communications using different way."""

    # PUBLIC

    def get(self, url: HttpUrl, callbacks: Callbacks | None = None) -> str:
        """Sends to given url given data synchronously as GET."""
        return self.__send(url, "GET", callbacks or default_callback()).body

    def post(self, url: HttpUrl, data: str | None = None, callbacks: Callbacks | None = None) -> str:
        """Sends to given url given data synchronously as POST."""
        return self.__send(url, "POST", callbacks or default_callback(), data).body

    def put(self, url: HttpUrl, data: str | None = None, callbacks: Callbacks | None = None) -> str:
        """Sends to given url given data synchronously as PUT."""
        return self.__send(url, "PUT", callbacks or default_callback(), data).body

    def send(
        self,
        url: HttpUrl,
        method: Methods,
        data: str | None = None,
        callbacks: Callbacks | None = None,
        timings: RequestTimings | None = None,
    ) -> Response:
        """
        Sends to given url given data synchronously.

        When preparing, sending or processing the request raises, callbacks of errors are called in their order and the
        exception is raised afterwards. A callback of errors may raise another exception instead (e.g. to translate
        the error) - it replaces the exception and the next callbacks of errors are not called.

        Args:
            url: Target of the request.
            method: HTTP method.
            data: Body of the request.
            callbacks: Callbacks of the request.
            timings: Timings of previous attempts of the request - duration of this attempt is appended to them.
        """
        return self.__send(url, method, callbacks or default_callback(), data, timings)

    async def async_get(self, url: HttpUrl, callbacks: AsyncCallbacks | None = None) -> str:
        """Sends to given url given data asynchronously as GET."""
        return (await self.__async_send(url, "GET", callbacks or default_async_callback())).body

    async def async_post(self, url: HttpUrl, data: str | None = None, callbacks: AsyncCallbacks | None = None) -> str:
        """Sends to given url given data asynchronously as POST."""
        return (await self.__async_send(url, "POST", callbacks or default_async_callback(), data)).body

    async def async_put(self, url: HttpUrl, data: str | None = None, callbacks: AsyncCallbacks | None = None) -> str:
        """Sends to given url given data asynchronously as PUT."""
        return (await self.__async_send(url, "PUT", callbacks or default_async_callback(), data)).body

    async def async_send(
        self,
        url: HttpUrl,
        method: Methods,
        data: str | None = None,
        callbacks: AsyncCallbacks | None = None,
        timings: RequestTimings | None = None,
    ) -> Response:
        """Sends to given url given data asynchronously - as `send`."""
        return await self.__async_send(url, method, callbacks or default_async_callback(), data, timings)

    # ABSTRACT

    @abstractmethod
    def teardown(self) -> None:
        """Called when work with communicator is over."""

    async def async_teardown(self) -> None:
        """Async version of teardown. Override for communicators requiring async cleanup."""
        self.teardown()

    @abstractmethod
    def _send(self, request: Request) -> Response:
        """Sends to given url given data synchronously."""

    @abstractmethod
    async def _async_send(self, request: Request) -> Response:
        """Sends to given url given data asynchronously."""

    # PROTECTED

    @classmethod
    def _encode_data(cls, data: str) -> bytes:
        return data.encode("utf-8")

    @classmethod
    def _decode_data(cls, data: bytes) -> str:
        return data.decode("utf-8")

    @classmethod
    def _default_headers(cls) -> dict[str, str]:
        """Returns headers for json communication."""
        return {"Content-Type": "application/json", "accept": "application/json"}

    def _construct_timeout_exception(self, request: Request) -> TimeoutExceededError:
        return TimeoutExceededError(url=request.url, request=request.body or "", timeout_secs=request.get_timeout())

    def _prepare_request(self, url: HttpUrl, method: Methods, data: str | None = None) -> Request:
        """Default prepare request callback."""
        return Request(
            url=url,
            method=method,
            headers=self._default_headers(),
            body=data,
            timeout=self.settings.timeout,
        )

    def _prepare_response(self, status_code: int, headers: dict[str, str], response: str) -> Response:
        """Default process response callback."""
        return Response(status_code=status_code, headers=headers, body=response)

    # PRIVATE

    def __send(
        self,
        url: HttpUrl,
        method: Methods,
        callbacks: Callbacks,
        data: str | None = None,
        timings: RequestTimings | None = None,
    ) -> Response:
        timings = RequestTimings() if timings is None else timings
        request = self._prepare_request(url, method, data)
        response: Response | None = None
        try:
            for prepare_request in callbacks.prepare_request:
                request = prepare_request(request)

            started = time.perf_counter()
            try:
                response = self._send(request)
            finally:
                timings.attempts.append(time.perf_counter() - started)

            context = RequestContext(request=request, response=response, timings=timings)
            for process_response in callbacks.process_response:
                process_response(context)
        except Exception as error:
            context = RequestContext(request=request, response=response, exception=error, timings=timings)
            for error_callback in callbacks.error:
                error_callback(context)
            raise

        return response

    async def __async_send(
        self,
        url: HttpUrl,
        method: Methods,
        callbacks: AsyncCallbacks,
        data: str | None = None,
        timings: RequestTimings | None = None,
    ) -> Response:
        timings = RequestTimings() if timings is None else timings
        request = self._prepare_request(url, method, data)
        response: Response | None = None
        try:
            for prepare_request in callbacks.prepare_request:
                request = await _resolve(prepare_request(request))

            started = time.perf_counter()
            try:
                response = await self._async_send(request)
            finally:
                timings.attempts.append(time.perf_counter() - started)

            context = RequestContext(request=request, response=response, timings=timings)
            for process_response in callbacks.process_response:
                await _resolve(process_response(context))
        except Exception as error:
            context = RequestContext(request=request, response=response, exception=error, timings=timings)
            for error_callback in callbacks.error:
                await _resolve(error_callback(context))
            raise

        return response


async def _resolve(result: T | Awaitable[T]) -> T:
    """Result of a callback - awaited when the callback is asynchronous (also an object with async `__call__`)."""
    if inspect.isawaitable(result):
        return cast("T", await result)
    return result
