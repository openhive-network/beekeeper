from __future__ import annotations

import json
import subprocess
import sys
from datetime import timedelta
from typing import TYPE_CHECKING, Final, NoReturn

import pytest
from beekeepy._communication.rules import (
    ApiNotFound,
    ErrorMessageRule,
    InvalidPassword,
    JussiResponse,
    NullResult,
    UnableToAcquireDatabaseLock,
    UnableToAcquireForkdbLock,
    UnableToOpenWallet,
    UnlockIsNotAccessible,
    WalletIsAlreadyUnlocked,
)
from beekeepy._remote_handle.app_status_probe import AppStatusProbe
from beekeepy.communication import (
    Abort,
    AsyncCallbacks,
    BeekeeperRules,
    Callbacks,
    ErrorInResponseRules,
    HiveRules,
    JsonRpcRules,
    Request,
    RequestContext,
    Response,
    Retry,
    SchemaValidationRules,
)
from beekeepy.exceptions import (
    ApiNotFoundError,
    DifferenceBetweenAmountOfRequestsAndResponsesError,
    ErrorInResponseError,
    GroupedErrorsError,
    JussiResponseError,
    MissingValidationModelWarning,
    NullResultError,
    OverseerError,
    OverseerInvalidPasswordError,
    SchemaValidationError,
    UnableToAcquireDatabaseLockError,
    UnableToAcquireForkdbLockError,
    UnableToOpenWalletError,
    UnlockIsNotAccessibleError,
    UnparsableResponseError,
    WalletIsAlreadyUnlockedError,
)
from beekeepy.handle.remote import (
    AbstractAsyncHandle,
    AbstractSyncHandle,
    AppStatusProbeAsyncApiCollection,
    AppStatusProbeSyncApiCollection,
    Beekeeper,
    RemoteHandleSettings,
)
from beekeepy.interfaces import HttpUrl
from hiveio_api import UnknownEndpointError
from local_tools.beekeepy.testing_server import run_simple_server

if TYPE_CHECKING:
    from collections.abc import Callable

    from beekeepy.communication import ResponseCallback
    from beekeepy.exceptions import Json

MAX_RETRIES: Final[int] = 2
REQUEST: Final[str] = """{"method": "aaa", "id": 1, "jsonrpc": "2.0"}"""
CORRECT_RESPONSE: Final[str] = """{"jsonrpc": "2.0", "result": {}, "id": 1}"""

RETRIED: Final[int] = 1 + MAX_RETRIES
"""Number of attempts of a request whose error is retried - sent again `max_retries` times."""
ABORTED: Final[int] = 1
"""Number of attempts of a request whose error aborts it."""

# error, response with the error, number of attempts of the request
ERRORS_TO_DETECT: Final[list[tuple[type[OverseerError], str, int]]] = [
    (
        NullResultError,
        """{"jsonrpc": "2.0", "result": null, "id": 1}""",
        RETRIED,
    ),
    (
        ApiNotFoundError,
        """{"jsonrpc": "2.0", "error": {"code": -32003, "message":
        "Assert Exception:api_itr != data._registered_apis.end(): Could not find API debug_node_api"
        }, "id": 1}""",
        ABORTED,
    ),
    (
        JussiResponseError,
        """{"jsonrpc":"2.0","id":null,"error":{"code":-32603,"message":
        "Internal Error","data":{"error_id":"b6384d8c-95ad-4af0-92dc-dd7828d3c707",
        "jussi_request_id":"000312363819934224"}}}""",
        RETRIED,
    ),
    (UnparsableResponseError, """404: Not Found""", RETRIED),
    (DifferenceBetweenAmountOfRequestsAndResponsesError, f"[{CORRECT_RESPONSE}]", RETRIED),
    (
        ErrorInResponseError,
        """{"jsonrpc": "2.0", "error": {"code": -32000, "message": "Some other error"}, "id": 1}""",
        ABORTED,
    ),
]
ERRORS_TO_DETECT_IDS: Final[list[str]] = [error.__name__ for error, *_ in ERRORS_TO_DETECT]


def all_rules() -> list[ResponseCallback]:
    return [JsonRpcRules(), HiveRules(), BeekeeperRules(), ErrorInResponseRules()]


def recording_attempts(attempts: list[int]) -> ResponseCallback:
    """Callback of responses recording numbers of attempts."""

    def record(context: RequestContext, /) -> None:
        attempts.append(context.attempt)

    return record


def settings(url: HttpUrl) -> RemoteHandleSettings:
    return RemoteHandleSettings(http_endpoint=url, max_retries=MAX_RETRIES, period_between_retries=timedelta(0))


class SyncRulesHandle(AbstractSyncHandle[RemoteHandleSettings, AppStatusProbeSyncApiCollection]):
    """Handle checking responses with the given rules."""

    def __init__(self, url: HttpUrl, rules: list[ResponseCallback]) -> None:
        self.__rules = rules
        super().__init__(settings=settings(url))

    def _rules(self) -> list[ResponseCallback]:
        return self.__rules

    def _construct_api(self) -> AppStatusProbeSyncApiCollection:
        return self._with_rules(AppStatusProbeSyncApiCollection(owner=self))

    def _target_service(self) -> str:
        return "test"

    def batch(self, *, delay_error_on_data_access: bool = False) -> NoReturn:
        raise NotImplementedError


class AsyncRulesHandle(AbstractAsyncHandle[RemoteHandleSettings, AppStatusProbeAsyncApiCollection]):
    """Handle checking responses with the given rules."""

    def __init__(self, url: HttpUrl, rules: list[ResponseCallback]) -> None:
        self.__rules = rules
        super().__init__(settings=settings(url))

    def _rules(self) -> list[ResponseCallback]:
        return self.__rules

    def _construct_api(self) -> AppStatusProbeAsyncApiCollection:
        return self._with_rules(AppStatusProbeAsyncApiCollection(owner=self))

    def _target_service(self) -> str:
        return "test"

    async def batch(self, *, delay_error_on_data_access: bool = False) -> NoReturn:
        raise NotImplementedError


def test_logs_of_handles_tell_whether_they_are_asynchronous() -> None:
    # ARRANGE
    sync_handle = SyncRulesHandle(HttpUrl("127.0.0.1:1"), [])
    async_handle = AsyncRulesHandle(HttpUrl("127.0.0.1:1"), [])

    # ACT & ASSERT
    assert sync_handle._logger_extras()["asynchronous"] is False
    assert async_handle._logger_extras()["asynchronous"] is True


def send(response: str, rules: list[ResponseCallback], request: str = REQUEST) -> Json | list[Json]:
    with run_simple_server(response) as url:
        handle = SyncRulesHandle(url, rules)
        try:
            return handle._send_batch(url=url, data=request, callbacks=Callbacks())
        finally:
            handle.teardown()


async def async_send(response: str, rules: list[ResponseCallback]) -> Json | list[Json]:
    with run_simple_server(response) as url:
        handle = AsyncRulesHandle(url, rules)
        try:
            return await handle._async_send_batch(url=url, data=REQUEST, callbacks=AsyncCallbacks())
        finally:
            await handle.async_teardown()


@pytest.mark.parametrize(("error", "response", "attempts"), ERRORS_TO_DETECT, ids=ERRORS_TO_DETECT_IDS)
def test_rules_detect_errors(error: type[OverseerError], response: str, attempts: int) -> None:
    # ARRANGE
    sent: list[int] = []

    # ACT
    with pytest.raises(error) as detected:
        send(response, [recording_attempts(sent), *all_rules()])

    # ASSERT
    assert len(sent) == attempts, "retried errors are sent again, errors aborting the request are not"
    assert isinstance(detected.value.cause, GroupedErrorsError), "all detected errors are given as the cause"


@pytest.mark.parametrize(("error", "response", "attempts"), ERRORS_TO_DETECT, ids=ERRORS_TO_DETECT_IDS)
async def test_rules_detect_errors_of_async_handle(error: type[OverseerError], response: str, attempts: int) -> None:
    # ARRANGE
    sent: list[int] = []

    # ACT
    with pytest.raises(error):
        await async_send(response, [recording_attempts(sent), *all_rules()])

    # ASSERT
    assert len(sent) == attempts


SECOND_REQUEST: Final[str] = """{"method": "bbb", "id": 2, "jsonrpc": "2.0"}"""
SECOND_RESPONSE: Final[str] = """{"jsonrpc": "2.0", "result": {}, "id": 2}"""

# request, response, message of the error
RESPONSES_NOT_ANSWERING_REQUESTS_ONE_TO_ONE: Final[list[tuple[str, str, str]]] = [
    (REQUEST, f"[{CORRECT_RESPONSE}]", "Received list of responses (batch), while requesting singular response"),
    (f"[{REQUEST}]", "[]", "Received no responses (empty array)"),
    (f"[{REQUEST}]", f"[{CORRECT_RESPONSE}, {SECOND_RESPONSE}]", "Received more responses than requested"),
    (f"[{REQUEST}, {SECOND_REQUEST}]", f"[{CORRECT_RESPONSE}]", "Received less responses than requested"),
]


@pytest.mark.parametrize(
    ("request_", "response", "message"),
    RESPONSES_NOT_ANSWERING_REQUESTS_ONE_TO_ONE,
    ids=["batch-for-single-request", "no-responses", "more-responses", "less-responses"],
)
def test_responses_not_answering_requests_one_to_one_are_retried(request_: str, response: str, message: str) -> None:
    # ARRANGE
    sent: list[int] = []

    # ACT
    with pytest.raises(DifferenceBetweenAmountOfRequestsAndResponsesError) as detected:
        send(response, [recording_attempts(sent), *all_rules()], request=request_)

    # ASSERT
    assert len(sent) == RETRIED
    assert detected.value.message_raw == message
    assert detected.value.whole_response == json.loads(response)


def test_batch_answered_one_to_one_is_accepted() -> None:
    # ARRANGE
    response = f"[{SECOND_RESPONSE}, {CORRECT_RESPONSE}]"

    # ACT
    result = send(response, all_rules(), request=f"[{REQUEST}, {SECOND_REQUEST}]")

    # ASSERT
    assert result == json.loads(response), "responses of a batch may come in any order"


NULL_RESULT: Final[str] = """{"jsonrpc": "2.0", "result": null, "id": %d}"""
EXCLUDED_REQUEST: Final[str] = """{"method": "condenser_api.get_escrow", "id": %d, "jsonrpc": "2.0"}"""


@pytest.mark.parametrize("method", NullResult.EXCLUDED_METHODS)
def test_null_results_of_excluded_methods_are_accepted(method: str) -> None:
    # ARRANGE
    sent: list[int] = []
    request = json.dumps({"method": method, "id": 1, "jsonrpc": "2.0"})

    # ACT
    result = send(NULL_RESULT % 1, [recording_attempts(sent), *all_rules()], request=request)

    # ASSERT
    assert result == json.loads(NULL_RESULT % 1)
    assert sent == [1], "not retried"


def test_null_results_of_batch_are_matched_with_excluded_methods_by_id() -> None:
    # ARRANGE - the excluded method is the second call of the batch, its response is the first one
    request = f"[{REQUEST}, {EXCLUDED_REQUEST % 2}]"
    response = f"[{NULL_RESULT % 2}, {CORRECT_RESPONSE}]"

    # ACT
    result = send(response, all_rules(), request=request)

    # ASSERT
    assert result == json.loads(response)


def test_whole_batch_is_retried_when_null_result_is_not_of_excluded_method() -> None:
    # ARRANGE - null result of the first call (not excluded), the excluded method is the second one
    sent: list[int] = []
    request = f"[{REQUEST}, {EXCLUDED_REQUEST % 2}]"
    response = f"[{NULL_RESULT % 2}, {NULL_RESULT % 1}]"

    # ACT
    with pytest.raises(NullResultError) as detected:
        send(response, [recording_attempts(sent), *all_rules()], request=request)

    # ASSERT
    assert sent == list(range(1, RETRIED + 1)), "the whole batch is sent again"
    assert detected.value.request_id == 1
    assert isinstance(detected.value.cause, GroupedErrorsError)
    assert detected.value.cause.exceptions == [detected.value], "only the call of a not excluded method failed"


def test_errors_of_all_responses_of_batch_are_grouped() -> None:
    # ARRANGE
    error = """{"jsonrpc": "2.0", "error": {"code": -32000, "message": "Some other error"}, "id": %d}"""
    response = f"[{error % 1}, {error % 2}]"

    # ACT
    with pytest.raises(ErrorInResponseError) as detected:
        send(response, all_rules(), request=f"[{REQUEST}, {SECOND_REQUEST}]")

    # ASSERT
    assert detected.value.request_id == 1, "the error of the first response is raised"
    grouped = detected.value.cause
    assert isinstance(grouped, GroupedErrorsError)
    assert grouped.get_exception_for(request_id=1) is detected.value
    assert isinstance(grouped.get_exception_for(request_id=2), ErrorInResponseError)


def send_with_callbacks(response: str, *callbacks: ResponseCallback) -> list[int]:
    """Send the request with the given callbacks of responses - returns numbers of attempts."""
    attempts: list[int] = []

    def count_attempts(context: RequestContext, /) -> None:
        attempts.append(context.attempt)

    with run_simple_server(response) as url:
        handle = SyncRulesHandle(url, [])
        try:
            handle._send_request(
                url=url,
                method="POST",
                data=REQUEST,
                callbacks=Callbacks(process_response=[count_attempts, *callbacks]),
            )
        finally:
            handle.teardown()
    return attempts


def test_request_is_sent_again_while_callbacks_raise_retry() -> None:
    # ARRANGE
    unlimited_retries = MAX_RETRIES + 2

    def retry_unlimited(context: RequestContext, /) -> None:
        if context.attempt <= unlimited_retries:
            raise Retry(UnableToAcquireDatabaseLockError(url="", request="", request_id=None), unlimited=True)

    # ACT
    attempts = send_with_callbacks(CORRECT_RESPONSE, retry_unlimited)

    # ASSERT
    assert attempts == list(range(1, unlimited_retries + 2))


def test_unlimited_retries_do_not_restore_limited_retries() -> None:
    # ARRANGE
    attempts: list[int] = []

    def alternate_retries(context: RequestContext, /) -> None:
        attempts.append(context.attempt)
        if context.attempt > 2 * (MAX_RETRIES + 1):  # the request would be retried forever
            return
        if context.attempt % 2 == 0:
            raise Retry(UnableToAcquireDatabaseLockError(url="", request="", request_id=None), unlimited=True)
        raise Retry(NullResultError(url="", request="", request_id=None))

    # ACT
    with pytest.raises(NullResultError):
        send_with_callbacks(CORRECT_RESPONSE, alternate_retries)

    # ASSERT
    assert attempts == list(range(1, 2 * MAX_RETRIES + 2)), "limited retries are counted between unlimited ones"


def test_unlimited_retries_are_sent_at_most_for_their_timeout() -> None:
    # ARRANGE
    timeout = timedelta(seconds=0.5)
    attempts: list[int] = []

    def retry_unlimited(context: RequestContext, /) -> None:
        attempts.append(context.attempt)
        if len(attempts) < 1000:  # noqa: PLR2004  # the request would be retried forever
            raise Retry(UnableToAcquireDatabaseLockError(url="", request="", request_id=None), unlimited=True)

    # ACT
    with run_simple_server(CORRECT_RESPONSE) as url:
        handle = SyncRulesHandle(url, [])
        with handle.update_settings() as updated:
            updated.period_between_retries = timedelta(seconds=0.05)
            updated.unlimited_retries_timeout = timeout
        try:
            with pytest.raises(UnableToAcquireDatabaseLockError):
                handle._send_request(
                    url=url, method="POST", data=REQUEST, callbacks=Callbacks(process_response=[retry_unlimited])
                )
        finally:
            handle.teardown()

    # ASSERT
    assert 1 < len(attempts) <= timeout / timedelta(seconds=0.05) + 1


def test_limited_retries_are_sent_at_most_max_retries_times() -> None:
    # ARRANGE
    attempts: list[int] = []

    def count_and_retry(context: RequestContext, /) -> None:
        attempts.append(context.attempt)
        raise Retry(NullResultError(url="", request="", request_id=None))

    # ACT
    with pytest.raises(NullResultError):
        send_with_callbacks(CORRECT_RESPONSE, count_and_retry)

    # ASSERT
    assert attempts == list(range(1, MAX_RETRIES + 2))


def test_abort_raises_first_error_caused_by_all_of_them() -> None:
    # ARRANGE
    first = ApiNotFoundError(url="", request="", request_id=1)
    second = ApiNotFoundError(url="", request="", request_id=2)

    def abort(_: RequestContext, /) -> NoReturn:
        raise Abort(first, second)

    # ACT
    with pytest.raises(ApiNotFoundError) as detected:
        send_with_callbacks(CORRECT_RESPONSE, abort)

    # ASSERT
    assert detected.value is first
    assert isinstance(detected.value.cause, GroupedErrorsError)
    assert detected.value.cause.exceptions == [first, second]


def test_other_exceptions_of_callbacks_are_raised_as_they_are() -> None:
    # ARRANGE
    def fail(_: RequestContext, /) -> NoReturn:
        raise ValueError("raised by a callback")

    # ACT & ASSERT
    with pytest.raises(ValueError, match="raised by a callback"):
        send_with_callbacks(CORRECT_RESPONSE, fail)


def test_results_of_jsonrpc_calls_are_taken_from_responses() -> None:
    # ARRANGE - a JSON-RPC call is recognized by its structure (as by rules), not by its formatting
    request = """{"jsonrpc" : "2.0", "id" : 1, "method" : "aaa"}"""

    # ACT
    with run_simple_server(CORRECT_RESPONSE) as url:
        handle = SyncRulesHandle(url, all_rules())
        try:
            response = handle._send(method="POST", expected_type=dict[str, int], data=request)
        finally:
            handle.teardown()

    # ASSERT
    assert response.result == {}


def test_responses_of_rest_requests_are_not_checked() -> None:
    # ARRANGE
    response = """{"error": "not a JSON-RPC error", "result": null}"""

    # ACT
    with run_simple_server(response) as url:
        handle = SyncRulesHandle(url, all_rules())
        try:
            received = handle._send_request(
                url=url, method="POST", data=None, callbacks=Callbacks(process_response=all_rules())
            )
        finally:
            handle.teardown()

    # ASSERT
    assert received.body == response


async def test_async_callbacks_of_responses_can_raise_retry() -> None:
    # ARRANGE
    attempts: list[int] = []

    async def retry_once(context: RequestContext, /) -> None:
        attempts.append(context.attempt)
        if context.attempt == 1:
            raise Retry(NullResultError(url="", request="", request_id=None))

    # ACT
    with run_simple_server(CORRECT_RESPONSE) as url:
        handle = AsyncRulesHandle(url, [])
        try:
            await handle._async_send_request(
                url=url, method="POST", data=REQUEST, callbacks=AsyncCallbacks(process_response=[retry_once])
            )
        finally:
            await handle.async_teardown()

    # ASSERT
    assert attempts == [1, 2]


VALID_ACCOUNT: Final[str] = """{"jsonrpc": "2.0", "result": {"accounts": [["alice"]]}, "id": 1}"""
INVALID_ACCOUNT: Final[str] = """{"jsonrpc": "2.0", "result": {"accounts": [["Invalid Name!"]]}, "id": 1}"""
# validated against the validation models of hiveio_api (account names of the result are checked)
SCHEMA_REQUEST: Final[str] = """{"method": "account_by_key_api.get_key_references", "id": 1, "jsonrpc": "2.0"}"""


def test_schema_validation_rules_detect_response_not_matching_schema() -> None:
    # ACT
    with pytest.raises(SchemaValidationError) as error:
        send(INVALID_ACCOUNT, [*all_rules(), SchemaValidationRules()], request=SCHEMA_REQUEST)

    # ASSERT
    assert [schema_error.path for schema_error in error.value.schema_errors] == ["$.accounts[0][0]"]


def rules_of_handle(handle_cls: type[AppStatusProbe | Beekeeper]) -> list[ResponseCallback]:
    handle = handle_cls(settings=RemoteHandleSettings(http_endpoint=HttpUrl("127.0.0.1:1")))
    try:
        return handle._rules()
    finally:
        handle.teardown()


def validating_rules() -> list[ResponseCallback]:
    return [*all_rules(), SchemaValidationRules()]


@pytest.mark.parametrize(
    ("rules", "request_", "response"),
    [
        (validating_rules, SCHEMA_REQUEST, VALID_ACCOUNT),
        (lambda: rules_of_handle(AppStatusProbe), SCHEMA_REQUEST, INVALID_ACCOUNT),
        (lambda: rules_of_handle(Beekeeper), SCHEMA_REQUEST, INVALID_ACCOUNT),
    ],
    ids=[
        "matching-schema",
        "rules-of-handles-do-not-validate",
        "rules-of-beekeeper-do-not-validate",
    ],
)
def test_schema_is_not_validated_or_matches(
    rules: Callable[[], list[ResponseCallback]], request_: str, response: str
) -> None:
    # ACT
    result = send(response, rules(), request=request_)

    # ASSERT
    assert result == json.loads(response)


@pytest.mark.parametrize("method", ["unknown_api.get_accounts", "account_by_key_api.unknown_method", "aaa"])
def test_responses_of_endpoints_without_validation_models_are_not_validated_with_warning(method: str) -> None:
    # ARRANGE
    rules = [*all_rules(), SchemaValidationRules()]
    request = json.dumps({"method": method, "id": 1, "jsonrpc": "2.0"})

    # ACT
    with pytest.warns(MissingValidationModelWarning) as warned:
        results = [send(INVALID_ACCOUNT, rules, request=request) for _ in range(2)]

    # ASSERT
    assert results == [json.loads(INVALID_ACCOUNT)] * 2
    assert len(warned) == 1, "each API without validation models is reported once"


@pytest.mark.parametrize("method", ["unknown_api.get_accounts", "account_by_key_api.unknown_method", "aaa"])
def test_strict_schema_validation_rejects_endpoints_without_validation_models(method: str) -> None:
    # ARRANGE
    request = json.dumps({"method": method, "id": 1, "jsonrpc": "2.0"})

    # ACT & ASSERT
    with pytest.raises(UnknownEndpointError):
        send(VALID_ACCOUNT, [*all_rules(), SchemaValidationRules(strict=True)], request=request)


def test_strict_schema_validation_accepts_responses_matching_schema() -> None:
    # ACT
    result = send(VALID_ACCOUNT, [*all_rules(), SchemaValidationRules(strict=True)], request=SCHEMA_REQUEST)

    # ASSERT
    assert result == json.loads(VALID_ACCOUNT)


def assert_exception(message: str, *, format_: str = "${msg}", line: int = 60) -> dict[str, object]:
    """Error of `FC_ASSERT(false, ...)` in a response of beekeeper - as sent by beekeeper (`throw_api_error`)."""
    return {
        "code": -32003,
        "message": f"Assert Exception:false: {message}",
        "data": {
            "code": 10,
            "name": "assert_exception",
            "message": "Assert Exception",
            "stack": [
                {
                    "context": {"level": "error", "file": "beekeeper_wallet_api.cpp", "line": line, "method": "unlock"},
                    "format": format_,
                    "data": {"msg": message} if format_ == "${msg}" else {},
                }
            ],
            "extension": {"assertion_expression": "false"},
        },
    }


# rule, error, `error` of the response (as sent by hived, beekeeper or jussi), message of the error
ERROR_MESSAGES: Final[list[tuple[type[ErrorMessageRule], type[OverseerError], dict[str, object], str]]] = [
    (
        UnableToAcquireDatabaseLock,
        UnableToAcquireDatabaseLockError,
        {"code": -32003, "message": "Unable to acquire database lock"},
        "Found `Unable to acquire database lock` in response",
    ),
    (
        UnableToAcquireForkdbLock,
        UnableToAcquireForkdbLockError,
        {"code": -32003, "message": "Unable to acquire forkdb lock"},
        "Found `Unable to acquire forkdb lock` in response",
    ),
    (
        ApiNotFound,
        ApiNotFoundError,
        {
            "code": -32002,
            "message": "Assert Exception:api_itr != data._registered_apis.end(): Could not find API debug_node_api",
            "data": {
                "code": 10,
                "name": "assert_exception",
                "message": "Assert Exception",
                "stack": [{"format": "Could not find API ${api}", "data": {"api": "debug_node_api"}}],
                "extension": {"assertion_expression": "api_itr != data._registered_apis.end()"},
            },
        },
        "Requested api not found: debug_node_api",
    ),
    (
        JussiResponse,
        JussiResponseError,
        {
            "code": -32603,
            "message": "Internal Error",
            "data": {"error_id": "b6384d8c-95ad-4af0-92dc-dd7828d3c707", "jussi_request_id": "000312363819934224"},
        },
        "Jussi responded instead of target service",
    ),
    (
        UnlockIsNotAccessible,
        UnlockIsNotAccessibleError,
        assert_exception("unlock is not accessible", format_="unlock is not accessible", line=238),
        "You tried to unlock wallet too fast",
    ),
    (
        WalletIsAlreadyUnlocked,
        WalletIsAlreadyUnlockedError,
        assert_exception("Wallet is already unlocked: alice-1"),
        "You tried to unlock already unlocked wallet: `alice-1`",
    ),
    (
        UnableToOpenWallet,
        UnableToOpenWalletError,
        assert_exception("Unable to open file"),
        "No such wallet: unknown",
    ),
    (
        UnableToOpenWallet,
        UnableToOpenWalletError,
        {
            "code": -32000,
            "message": "Unknown error - parsing rpc message failed",
            "data": "Wallet file not found: /home/user/beekeeper/./bob.wallet",
        },
        "No such wallet: bob",
    ),
    (
        InvalidPassword,
        OverseerInvalidPasswordError,
        assert_exception("Invalid password for wallet: alice-1"),
        "Invalid password for wallet: alice-1",
    ),
]

ERROR_MESSAGES_IDS: Final[list[str]] = [
    "UnableToAcquireDatabaseLock",
    "UnableToAcquireForkdbLock",
    "ApiNotFound",
    "JussiResponse",
    "UnlockIsNotAccessible",
    "WalletIsAlreadyUnlocked",
    "UnableToOpenWallet-unable-to-open-file",
    "UnableToOpenWallet-wallet-file-not-found",
    "InvalidPassword",
]


def context_of(response: dict[str, object]) -> RequestContext:
    return RequestContext(
        request=Request(url=HttpUrl("http://127.0.0.1:8090"), method="POST", headers={}, body=REQUEST),
        response=Response(status_code=200, headers={}, body=json.dumps(response)),
    )


def check_rule(rule_cls: type[ErrorMessageRule], response: dict[str, object]) -> list[OverseerError]:
    return rule_cls().check(context_of(response))


@pytest.mark.parametrize("message", ["Unable to acquire database lock", "Unable to acquire forkdb lock"])
def test_locks_are_retried_without_limit(message: str) -> None:
    # ARRANGE
    context = context_of({"jsonrpc": "2.0", "error": {"code": -32003, "message": message}, "id": 1})

    # ACT
    with pytest.raises(Retry) as signal:
        HiveRules()(context)

    # ASSERT
    assert signal.value.unlimited


def response_with_custom_json(text: str) -> dict[str, object]:
    """A correct response holding the text in data of a user (json of a custom_json operation)."""
    operation = {"type": "custom_json_operation", "value": {"id": "app", "json": json.dumps({"note": text})}}
    return {"jsonrpc": "2.0", "result": {"block": {"transactions": [{"operations": [operation]}]}}, "id": 1}


def detected_by_all_rules(context: RequestContext) -> list[Exception]:
    """Errors detected by rules of all handles checked in their order - by the first rule detecting a problem."""
    for rules in all_rules():
        try:
            rules(context)
        except (Abort, Retry) as signal:
            return signal.errors
    return []


@pytest.mark.parametrize(("rule_cls", "error", "response_error", "message"), ERROR_MESSAGES, ids=ERROR_MESSAGES_IDS)
def test_error_message_rules_detect_messages_in_error(
    rule_cls: type[ErrorMessageRule], error: type[OverseerError], response_error: dict[str, object], message: str
) -> None:
    # ARRANGE
    response: dict[str, object] = {"jsonrpc": "2.0", "error": response_error, "id": 1}

    # ACT
    detected = check_rule(rule_cls, response)

    # ASSERT
    assert [type(exception) for exception in detected] == [error]
    assert detected[0].message_raw == message
    assert message in str(detected[0])
    detected_first = detected_by_all_rules(context_of(response))
    assert [type(exception) for exception in detected_first] == [error], "no other rule detects it before"


@pytest.mark.parametrize(("rule_cls", "error", "response_error", "message"), ERROR_MESSAGES, ids=ERROR_MESSAGES_IDS)
def test_error_message_rules_ignore_messages_in_result(
    rule_cls: type[ErrorMessageRule],
    error: type[OverseerError],  # noqa: ARG001
    response_error: dict[str, object],
    message: str,  # noqa: ARG001  # a column of the table shared with the test of detection
) -> None:
    # ACT
    detected = check_rule(rule_cls, response_with_custom_json(json.dumps(response_error)))

    # ASSERT
    assert detected == []


def test_rules_accept_messages_of_errors_in_data_of_users() -> None:
    # ARRANGE
    response = response_with_custom_json("Wallet file not found, unlock is not accessible")

    # ACT & ASSERT
    assert send(json.dumps(response), all_rules()) == response


ERRORS_DETECTED_BY_ALL_HANDLES: Final[list[tuple[type[OverseerError], str, int]]] = [
    detected
    for detected in ERRORS_TO_DETECT
    if detected[0]
    in {
        ApiNotFoundError,
        UnparsableResponseError,
        DifferenceBetweenAmountOfRequestsAndResponsesError,
        ErrorInResponseError,
    }
]


@pytest.mark.parametrize("handle_cls", [AppStatusProbe, Beekeeper])
@pytest.mark.parametrize(
    ("error", "response", "attempts"),
    ERRORS_DETECTED_BY_ALL_HANDLES,
    ids=[error.__name__ for error, *_ in ERRORS_DETECTED_BY_ALL_HANDLES],
)
def test_rules_of_handles_detect_errors(
    handle_cls: type[AppStatusProbe | Beekeeper], error: type[OverseerError], response: str, attempts: int
) -> None:
    # ARRANGE
    sent: list[int] = []

    # ACT
    with pytest.raises(error) as detected:
        send(response, [recording_attempts(sent), *rules_of_handle(handle_cls)])

    # ASSERT
    assert type(detected.value) is error, "rules of JSON-RPC are checked before the rule of any other error"
    assert len(sent) == attempts


def test_rules_are_exported_by_communication_without_other_imports() -> None:
    # ACT - in a new interpreter, nothing imported the module of rules before
    result = subprocess.run(
        [sys.executable, "-c", "from beekeepy.communication import rules; print(rules.ApiNotFound.__name__)"],
        capture_output=True,
        text=True,
        check=False,
    )

    # ASSERT
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "ApiNotFound"
