from __future__ import annotations

from beekeepy._exceptions.base import BeekeepyError

__all__ = ["Abort", "Retry"]


class Retry(BeekeepyError):  # noqa: N818  # signal to the handle, not an error
    """
    Raised by callbacks of responses - the handle sends the request again.

    Limited retries are sent at most `max_retries` times (settings of the handle), then the first of `errors` is raised
    (caused by `GroupedErrorsError` with all of them).
    """

    def __init__(self, *errors: Exception, unlimited: bool = False) -> None:
        assert errors, "Retry needs the errors raised when no retry is left"
        super().__init__(*errors)
        self.errors = list(errors)
        self.unlimited = unlimited
        """
        Retry until the response is accepted (e.g. the node holds its database lock).

        Unlimited retries do not use `max_retries`, they are limited only by `unlimited_retries_timeout` of settings.
        """


class Abort(BeekeepyError):  # noqa: N818  # signal to the handle, not an error
    """
    Raised by callbacks of responses - the handle raises the first of `errors`.

    The raised error is caused by `GroupedErrorsError` with all errors (e.g. detected in all responses of a batch).
    """

    def __init__(self, *errors: Exception) -> None:
        assert errors, "Abort needs the errors to raise"
        super().__init__(*errors)
        self.errors = list(errors)
