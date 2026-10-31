from __future__ import annotations

from typing import TYPE_CHECKING, Any

from beekeepy.exceptions import BeekeepyError, CommunicationResponseT, Json, OverseerError

if TYPE_CHECKING:
    from collections.abc import Sequence

    from hiveio_api import SchemaError
    from hiveio_api.client import AnyUrl, Url


class UnableToAcquireDatabaseLockError(OverseerError):
    """Node could not acquire its database lock - the request is sent again until it succeeds."""


class UnableToAcquireForkdbLockError(OverseerError):
    """Node could not acquire its fork database lock - the request is sent again until it succeeds."""


class NullResultError(OverseerError):
    """`result` of the response is null."""


class ApiNotFoundError(OverseerError):
    @property
    def api(self) -> str:
        return self.message_raw.split(":")[-1].strip()


class JussiResponseError(OverseerError):
    """Jussi responded instead of the target service."""


class UnparsableResponseError(OverseerError):
    """Response is not parsable - neither a JSON-RPC object nor an array of them."""


class DifferenceBetweenAmountOfRequestsAndResponsesError(OverseerError):
    """Amount of responses differs from amount of requests of a batch."""


class UnlockIsNotAccessibleError(OverseerError):
    """Wallet cannot be unlocked yet - it was unlocked too fast."""


class WalletIsAlreadyUnlockedError(OverseerError):
    """Wallet is already unlocked."""


class UnableToOpenWalletError(OverseerError):
    """Wallet cannot be opened - there is no such wallet."""


class OverseerInvalidPasswordError(OverseerError):
    """Invalid password of the wallet - reported in a response of beekeeper (wallets raise `InvalidPasswordError`)."""


class ErrorInResponseError(OverseerError):
    def __init__(  # noqa: PLR0913
        self,
        url: str | Url[Any],
        request: CommunicationResponseT | bytes,
        response: CommunicationResponseT | None = None,
        whole_response: CommunicationResponseT | None = None,
        *,
        message: str = "",
        request_id: int | None,
    ) -> None:
        super().__init__(
            url=url,
            request=request,
            response=response,
            message=message,
            request_id=request_id,
            whole_response=whole_response,
        )
        self.__error: str | None = None

    @property
    def error(self) -> str:
        """
        Error of the response - its `data` and `message`, searched for known errors (`DetectableError`).

        Messages of the service are formatted in `message` (e.g. `Assert Exception:false: Public key STM... not found
        in w1 wallet`), while `data` holds the exception with templates of messages and their arguments - or the details
        of errors with a generic `message`.
        """
        if self.__error is None:
            response = self.__get_suitable_response()
            result = self._extract_error_messages(response=response)
            error = (result or [""])[0]
            message = self.__message_of(response)
            self.__error = error if message is None or message in error else f"{error}\n{message}"
        return self.__error

    @staticmethod
    def __message_of(response: Json | str | None) -> str | None:
        if not isinstance(response, dict):
            return None
        error = response.get("error")
        message = error.get("message") if isinstance(error, dict) else None
        return message if isinstance(message, str) else None

    def __get_suitable_response(self) -> Json | str | None:
        if isinstance(self.response, list):
            if self.request_id is None:
                return None
            for item in self.response:
                if item.get("id", {}) == self.request_id:
                    return item
            return None
        return self.response


class SchemaValidationError(OverseerError):
    """Result of the response does not match the validation model of the called endpoint."""

    def __init__(  # noqa: PLR0913
        self,
        url: str | AnyUrl,
        request: CommunicationResponseT | bytes,
        response: CommunicationResponseT | None = None,
        whole_response: CommunicationResponseT | None = None,
        *,
        message: str = "",
        request_id: int | None,
        schema_errors: Sequence[SchemaError] = (),
    ) -> None:
        super().__init__(
            url=url,
            request=request,
            response=response,
            message=message,
            request_id=request_id,
            whole_response=whole_response,
        )
        self.schema_errors = list(schema_errors)
        """Problems found by `hiveio_api.validate_schema` (`SchemaError` instances)."""


class MissingValidationModelWarning(UserWarning):
    """Responses of an API are not validated - it has no validation models (`SchemaValidationRules`)."""


class GroupedErrorsError(BeekeepyError):
    def __init__(self, exceptions: Sequence[BaseException]) -> None:
        self.exceptions = list(exceptions)

    def get_exception_for(self, *, request_id: int) -> OverseerError | None:
        for exception in self.exceptions:
            if isinstance(exception, OverseerError) and exception.request_id == request_id:
                return exception
        return None
