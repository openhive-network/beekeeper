from __future__ import annotations

import json
from abc import ABC, abstractmethod
from enum import Enum
from typing import TYPE_CHECKING, ClassVar, TypedDict, cast

from beekeepy._communication.signals import Abort, Retry

if TYPE_CHECKING:
    from hiveio_api.client import HttpUrl, JSONRPCRequest, RequestContext, Response

    from beekeepy.exceptions import CommunicationResponseT, Json, OverseerError


class Reaction(Enum):
    """What happens to the request when a rule detects a problem in its response."""

    ABORT = "abort"
    """The detected errors are raised."""

    RETRY = "retry"
    """The request is sent again, at most `max_retries` times - then the detected errors are raised."""

    RETRY_UNLIMITED = "retry_unlimited"
    """The request is sent again until the problem disappears (at most for `unlimited_retries_timeout` of settings)."""


class ErrorArguments(TypedDict):
    """Arguments of errors detected by rules (`OverseerError`)."""

    url: HttpUrl
    request: CommunicationResponseT
    request_id: int | None
    response: CommunicationResponseT
    whole_response: CommunicationResponseT
    message: str


class Rule(ABC):
    """Rule of responses - detects one kind of problem in the response of a request."""

    @abstractmethod
    def check(self, context: RequestContext) -> list[OverseerError]:
        """Errors detected in the response of the context - e.g. one for each response of a batch."""

    @classmethod
    @abstractmethod
    def expected_exception(cls) -> type[OverseerError]:
        """Type of errors detected by the rule."""

    def _error(
        self, context: RequestContext, *, response: CommunicationResponseT, request_id: int | None, message: str
    ) -> OverseerError:
        """Error of the rule detected in the response (or one of responses of a batch) of the context."""
        return self.expected_exception()(
            **self._error_arguments(context, response=response, request_id=request_id, message=message)
        )

    def _error_arguments(
        self, context: RequestContext, *, response: CommunicationResponseT, request_id: int | None, message: str
    ) -> ErrorArguments:
        """Arguments of the error - for rules raising errors with additional arguments."""
        return ErrorArguments(
            url=context.request.url,
            request=request_payload(context),
            request_id=request_id,
            response=response,
            whole_response=whole_response(context),
            message=message,
        )


class ResponsesRule(Rule):
    """
    Rule checking each JSON-RPC response of the context - the response, or each response of a batch.

    A response which is not parsable is not checked (`UnparsableResponse` detects it).
    """

    def check(self, context: RequestContext) -> list[OverseerError]:
        return [
            error
            for response in jsonrpc_responses(context)
            if (error := self._check_response(context, response)) is not None
        ]

    @abstractmethod
    def _check_response(self, context: RequestContext, response: Json) -> OverseerError | None:
        """Error detected in the response (one of responses of a batch), `None` when it is correct."""


class RuleSet:
    """
    Callback of responses (`process_response`) checking its rules in their order.

    The first rule detecting a problem decides - `Abort` or `Retry` is raised with all errors it detected (e.g. in all
    responses of a batch). Only responses of JSON-RPC requests are checked.
    """

    RULES: ClassVar[tuple[tuple[Rule, Reaction], ...]] = ()

    def __call__(self, context: RequestContext, /) -> None:
        if not context.request.jsonrpc or context.response is None:
            return
        for rule, reaction in self._rules():
            if not (errors := rule.check(context)):
                continue
            if reaction is Reaction.ABORT:
                raise Abort(*errors)
            raise Retry(*errors, unlimited=reaction is Reaction.RETRY_UNLIMITED)

    def _rules(self) -> tuple[tuple[Rule, Reaction], ...]:
        """Rules of the set with their reactions - `RULES`, override for rules of the instance."""
        return self.RULES


def received(context: RequestContext) -> Response:
    """Response of the context - rules check received responses."""
    assert context.response is not None, "Rules check received responses"
    return context.response


def jsonrpc_responses(context: RequestContext) -> list[Json]:
    """JSON-RPC responses of the context - the response, or responses of a batch; empty when it is not parsable."""
    try:
        payload = received(context).json
    except json.JSONDecodeError:
        return []
    items = payload if isinstance(payload, list) else [payload]
    return [cast("Json", item) for item in items if isinstance(item, dict)]


def response_id(response: Json) -> int | None:
    """Id of the JSON-RPC response - ids of requests sent by handles are numbers."""
    id_ = response.get("id")
    return id_ if isinstance(id_, int) else None


def matching_request(context: RequestContext, request_id: int | None) -> JSONRPCRequest | None:
    """JSON-RPC call answered by the response of the given id - the call of a single request, or of a batch."""
    calls = context.request.jsonrpc
    if not isinstance(context.request.json, list):
        return calls[0] if calls else None
    return next((call for call in calls if call.id_ == request_id), None)


def request_payload(context: RequestContext) -> Json | list[Json]:
    """Parsed JSON-RPC request of the context - a call, or calls of a batch."""
    return cast("Json | list[Json]", context.request.json)


def whole_response(context: RequestContext) -> CommunicationResponseT:
    """Parsed response of the context, its body when it is not parsable."""
    response = received(context)
    try:
        return cast("Json | list[Json]", response.json)
    except json.JSONDecodeError:
        return response.body
