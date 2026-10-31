from __future__ import annotations

import json
import re
import warnings
from abc import abstractmethod
from typing import TYPE_CHECKING, ClassVar, Final

from hiveio_api import UnknownEndpointError, validate_json

from beekeepy._communication.abc.rules import (
    Reaction,
    ResponsesRule,
    Rule,
    RuleSet,
    matching_request,
    received,
    request_payload,
    response_id,
    whole_response,
)
from beekeepy.exceptions import (
    ApiNotFoundError,
    DifferenceBetweenAmountOfRequestsAndResponsesError,
    ErrorInResponseError,
    JussiResponseError,
    MissingValidationModelWarning,
    NullResultError,
    OverseerInvalidPasswordError,
    SchemaValidationError,
    UnableToAcquireDatabaseLockError,
    UnableToAcquireForkdbLockError,
    UnableToOpenWalletError,
    UnlockIsNotAccessibleError,
    UnparsableResponseError,
    WalletIsAlreadyUnlockedError,
)

if TYPE_CHECKING:
    from collections.abc import Sequence

    from hiveio_api.client import RequestContext

    from beekeepy.exceptions import Json, OverseerError


REGEX_FOR_PATH_WITH_CAPTURE_GROUP_ON_WALLET_NAME: Final[str] = r"\/(?:(?:[^\/]+\/)+)([^\/]+)\.wallet"


class ErrorMessageRule(ResponsesRule):
    """
    Detects an error by its message - only the `error` member of a JSON-RPC response is searched.

    Messages of errors of hived and beekeeper are always in `error` (`message`, `data`). `result` holds data of users
    (posts, memos, custom_json), so searching the whole response detected errors in correct responses, and turned
    every response into a string once per pattern.
    """

    PATTERNS: ClassVar[Sequence[re.Pattern[str]]]
    """Patterns of the message, searched in their order - the first match decides."""

    def _check_response(self, context: RequestContext, response: Json) -> OverseerError | None:
        error = response.get("error")
        if error is None:
            return None
        error_text = self._error_text(error)
        for pattern in self.PATTERNS:
            if (match := pattern.search(error_text)) is not None:
                return self._error(
                    context, response=response, request_id=response_id(response), message=self._message(match)
                )
        return None

    def _error_text(self, error: object) -> str:
        """Text of the error searched by the patterns - the whole `error` member (message and data)."""
        return str(error)

    @abstractmethod
    def _message(self, match: re.Match[str]) -> str:
        """Message of the exception raised when a pattern matches."""


class UnableToAcquireDatabaseLock(ErrorMessageRule):
    """Node could not acquire its database lock (`UnableToAcquireDatabaseLockError`) - e.g. it is busy with a block."""

    LOOKUP_MESSAGE: ClassVar[str] = "Unable to acquire database lock"
    PATTERNS: ClassVar[Sequence[re.Pattern[str]]] = (re.compile(re.escape(LOOKUP_MESSAGE)),)

    def _message(self, match: re.Match[str]) -> str:  # noqa: ARG002
        return f"Found `{self.LOOKUP_MESSAGE}` in response"

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return UnableToAcquireDatabaseLockError


class UnableToAcquireForkdbLock(ErrorMessageRule):
    """Node could not acquire its fork database lock (`UnableToAcquireForkdbLockError`)."""

    LOOKUP_MESSAGE: ClassVar[str] = "Unable to acquire forkdb lock"
    PATTERNS: ClassVar[Sequence[re.Pattern[str]]] = (re.compile(re.escape(LOOKUP_MESSAGE)),)

    def _message(self, match: re.Match[str]) -> str:  # noqa: ARG002
        return f"Found `{self.LOOKUP_MESSAGE}` in response"

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return UnableToAcquireForkdbLockError


class NullResult(ResponsesRule):
    """
    `result` of a JSON-RPC response is null (`NullResultError`).

    Responses of `EXCLUDED_METHODS` are accepted - the method is taken from the matching request (of a batch by id).
    """

    EXCLUDED_METHODS: ClassVar[Sequence[str]] = (
        "wallet_bridge_api.get_account",
        "wallet_bridge_api.get_witness",
        "condenser_api.get_escrow",
    )
    """Endpoints returning null as a correct result."""

    def _check_response(self, context: RequestContext, response: Json) -> OverseerError | None:
        if "result" not in response or response["result"] is not None:
            return None
        request = matching_request(context, response_id(response))
        if request is not None and request.method in self.EXCLUDED_METHODS:
            return None
        return self._error(
            context, response=response, request_id=response_id(response), message="`result` field in response is null"
        )

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return NullResultError


class ApiNotFound(ErrorMessageRule):
    """Called API is not enabled in the service (`ApiNotFoundError` naming the API)."""

    PATTERNS: ClassVar[Sequence[re.Pattern[str]]] = (
        re.compile(r"Assert Exception:api_itr != data\._registered_apis\.end\(\): Could not find API (\w+_api)"),
    )

    def _message(self, match: re.Match[str]) -> str:
        return f"Requested api not found: {match.group(1)}"

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return ApiNotFoundError


class JussiResponse(ErrorMessageRule):
    """Jussi (a proxy in front of nodes) responded instead of the target service (`JussiResponseError`)."""

    PATTERNS: ClassVar[Sequence[re.Pattern[str]]] = (re.compile("jussi_request_id"),)

    def _message(self, match: re.Match[str]) -> str:  # noqa: ARG002
        return "Jussi responded instead of target service"

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return JussiResponseError


class UnparsableResponse(Rule):
    """Response is not a JSON-RPC response - neither an object nor an array of objects (batch)."""

    def check(self, context: RequestContext) -> list[OverseerError]:
        response = received(context)
        try:
            payload = response.json
        except json.JSONDecodeError as error:
            message = f"Received response is not parsable, probably plaintext or invalid json: {response.body}"
            return [self._error(context, response=str(error), request_id=None, message=message)]
        items = payload if isinstance(payload, list) else [payload]
        if all(isinstance(item, dict) for item in items):
            return []
        message = f"Received response is neither a JSON-RPC object nor an array of them: {response.body}"
        return [self._error(context, response=response.body, request_id=None, message=message)]

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return UnparsableResponseError


class DifferenceBetweenAmountOfRequestsAndResponses(Rule):
    """Responses of a batch do not answer its requests one to one."""

    def check(self, context: RequestContext) -> list[OverseerError]:
        try:
            payload = received(context).json
        except json.JSONDecodeError:
            return []  # detected by UnparsableResponse
        if not isinstance(payload, list):
            return []

        requests = request_payload(context)
        if not isinstance(requests, list):
            message = "Received list of responses (batch), while requesting singular response"
        elif not payload and requests:
            message = "Received no responses (empty array)"
        elif len(requests) < len(payload):
            message = "Received more responses than requested"
        elif len(requests) > len(payload):
            message = "Received less responses than requested"
        else:
            return []
        return [self._error(context, response=whole_response(context), request_id=None, message=message)]

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return DifferenceBetweenAmountOfRequestsAndResponsesError


class ErrorInResponse(ResponsesRule):
    """Any error in a JSON-RPC response (`ErrorInResponseError`) - checked after rules of known errors."""

    def _check_response(self, context: RequestContext, response: Json) -> OverseerError | None:
        if (error := response.get("error")) is None:
            return None
        return self._error(
            context, response=response, request_id=response_id(response), message=f"Error found in response: {error=}"
        )

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return ErrorInResponseError


class UnlockIsNotAccessible(ErrorMessageRule):
    """Beekeeper refused to unlock a wallet - it was unlocked too fast (`UnlockIsNotAccessibleError`)."""

    PATTERNS: ClassVar[Sequence[re.Pattern[str]]] = (re.compile("unlock is not accessible"),)

    def _message(self, match: re.Match[str]) -> str:  # noqa: ARG002
        return "You tried to unlock wallet too fast"

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return UnlockIsNotAccessibleError


class WalletIsAlreadyUnlocked(ErrorMessageRule):
    """Beekeeper refused to unlock a wallet which is already unlocked (`WalletIsAlreadyUnlockedError` naming it)."""

    PATTERNS: ClassVar[Sequence[re.Pattern[str]]] = (
        re.compile(r"_itr->is_locked\(\): Wallet with name: '([\w-]+)' is already unlocked"),
        re.compile(r"_itr->is_locked\(\): Wallet is already unlocked: ([\w-]+)rethrow"),
        re.compile(r"Wallet is already unlocked: ([\w-]+)"),
    )

    def _message(self, match: re.Match[str]) -> str:
        return f"You tried to unlock already unlocked wallet: `{match.group(1)}`"

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return WalletIsAlreadyUnlockedError


class UnableToOpenWallet(ErrorMessageRule):
    """Beekeeper could not open a wallet - there is no such wallet (`UnableToOpenWalletError` naming it if known)."""

    PATTERNS: ClassVar[Sequence[re.Pattern[str]]] = (
        re.compile(
            r"_new_item->load_wallet_file\(\): "
            r"Unable to open file: " + REGEX_FOR_PATH_WITH_CAPTURE_GROUP_ON_WALLET_NAME + r"(?:rethrow)?"
        ),
        re.compile(r"Wallet file not found: " + REGEX_FOR_PATH_WITH_CAPTURE_GROUP_ON_WALLET_NAME),
        re.compile(r"Unable to open file"),
        re.compile(r"Wallet file not found"),
    )

    def _message(self, match: re.Match[str]) -> str:
        return f"No such wallet: {match.group(1) if match.lastindex else 'unknown'}"

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return UnableToOpenWalletError


class InvalidPassword(ErrorMessageRule):
    """Beekeeper refused the password of a wallet (`OverseerInvalidPasswordError` naming the wallet)."""

    PATTERNS: ClassVar[Sequence[re.Pattern[str]]] = (
        re.compile(
            r"false: Invalid password for wallet: '"
            + REGEX_FOR_PATH_WITH_CAPTURE_GROUP_ON_WALLET_NAME
            + r"' (?:rethrow)?"
        ),
        re.compile(r"Invalid password for wallet: ([\w-]+)"),
    )

    def _error_text(self, error: object) -> str:
        """Only the message of the error (data of the error repeats it with paths of the wallet)."""
        message = error.get("message", "") if isinstance(error, dict) else ""
        return message if isinstance(message, str) else ""

    def _message(self, match: re.Match[str]) -> str:
        return f"Invalid password for wallet: {match.group(1)}"

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return OverseerInvalidPasswordError


_MAX_SCHEMA_ERRORS_IN_MESSAGE: Final[int] = 10


class SchemaValidation(ResponsesRule):
    """
    Validates `result` of a JSON-RPC response against the validation model of the called endpoint.

    Uses `hiveio_api.validate_json` (`result` is already JSON builtins, validated without a copy) with the endpoint
    taken from `method` of the matching request (e.g. `condenser_api.get_accounts`). Responses with an error, without
    (or with null) `result` and REST requests are skipped.

    Endpoints without validation models (`hiveio_api.UnknownEndpointError`) are skipped with a warning
    (`MissingValidationModelWarning`, once for each API), in the strict mode the error is raised.

    Note: Validation is opt-in, API calls do not validate responses by default - use `SchemaValidationRules`.
    """

    def __init__(self, *, strict: bool = False) -> None:
        self.__strict = strict
        self.__apis_without_models: set[str] = set()
        """APIs reported as having no validation models - each one is reported once."""

    def _check_response(self, context: RequestContext, response: Json) -> OverseerError | None:
        result = response.get("result")
        if result is None or response.get("error") is not None:
            return None
        request = matching_request(context, response_id(response))
        if request is None:
            return None

        try:
            schema_errors = validate_json(result, request.method)
        except UnknownEndpointError as error:
            if self.__strict:
                raise
            self.__warn_once(request.method, error)
            return None
        if not schema_errors:
            return None

        described = "\n".join(
            f"  {error.path}: {error.message}" for error in schema_errors[:_MAX_SCHEMA_ERRORS_IN_MESSAGE]
        )
        more = len(schema_errors) - _MAX_SCHEMA_ERRORS_IN_MESSAGE
        message = f"Response of `{request.method}` does not match its schema:\n{described}" + (
            f"\n  ... and {more} more" if more > 0 else ""
        )
        return SchemaValidationError(
            **self._error_arguments(context, response=response, request_id=response_id(response), message=message),
            schema_errors=schema_errors,
        )

    def __warn_once(self, method: str, error: UnknownEndpointError) -> None:
        api = method.rpartition(".")[0] or method
        if api in self.__apis_without_models:
            return
        self.__apis_without_models.add(api)
        warnings.warn(f"Responses of `{api}` are not validated: {error}", MissingValidationModelWarning, stacklevel=2)

    @classmethod
    def expected_exception(cls) -> type[OverseerError]:
        return SchemaValidationError


class JsonRpcRules(RuleSet):
    """
    Responses of JSON-RPC servers of appbase (hived, beekeeper) - structure of responses and not found APIs.

    The first rules of handles; errors of the services are checked by the next rule sets.
    """

    RULES = (
        (UnparsableResponse(), Reaction.RETRY),
        (DifferenceBetweenAmountOfRequestsAndResponses(), Reaction.RETRY),
        (ApiNotFound(), Reaction.ABORT),
    )


class HiveRules(RuleSet):
    """Errors of hived (and jussi in front of it)."""

    RULES = (
        (UnableToAcquireDatabaseLock(), Reaction.RETRY_UNLIMITED),
        (UnableToAcquireForkdbLock(), Reaction.RETRY_UNLIMITED),
        (JussiResponse(), Reaction.RETRY),
        (NullResult(), Reaction.RETRY),
    )


class BeekeeperRules(RuleSet):
    """Errors of beekeeper."""

    RULES = (
        (UnlockIsNotAccessible(), Reaction.ABORT),
        (WalletIsAlreadyUnlocked(), Reaction.ABORT),
        (UnableToOpenWallet(), Reaction.ABORT),
        (InvalidPassword(), Reaction.ABORT),
    )


class ErrorInResponseRules(RuleSet):
    """Any other error in a response - the last rules of handles."""

    RULES = ((ErrorInResponse(), Reaction.ABORT),)


class SchemaValidationRules(RuleSet):
    """
    Results of responses match validation models of called endpoints (`hiveio_api.validate_schema`).

    Opt-in - add after rules of the handle (e.g. `handle.apis.callbacks.process_response.append(...)`).

    Validation models of APIs of hived are registered by `hiveio_api`. Models of `beekeeper_api` are not a part of
    beekeepy - they are generated for its tests only, so responses of beekeeper are validated only after their
    registration (`hiveio_api.register_validation_models`).
    """

    def __init__(self, *, strict: bool = False) -> None:
        """
        Rules validating results of responses.

        Args:
            strict: Raise `hiveio_api.UnknownEndpointError` when a called endpoint has no validation model - otherwise
                its responses are not validated and `MissingValidationModelWarning` is issued once for each API.
        """
        self.__rules = ((SchemaValidation(strict=strict), Reaction.ABORT),)

    def _rules(self) -> tuple[tuple[Rule, Reaction], ...]:
        return self.__rules
