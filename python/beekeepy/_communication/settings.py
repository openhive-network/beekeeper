from __future__ import annotations

import copy
import json
import re
import warnings
from dataclasses import dataclass, field, fields
from datetime import timedelta
from os import environ
from pathlib import Path
from types import UnionType
from typing import TYPE_CHECKING, ClassVar, Final, Self, TypeVar, Union, get_args, get_origin, get_type_hints

if TYPE_CHECKING:
    from collections.abc import Callable

T = TypeVar("T")

_DURATION: Final[re.Pattern[str]] = re.compile(
    r"(?P<sign>-)?P(?:(?P<days>\d+(?:\.\d+)?)D)?"
    r"(?:T(?:(?P<hours>\d+(?:\.\d+)?)H)?(?:(?P<minutes>\d+(?:\.\d+)?)M)?(?:(?P<seconds>\d+(?:\.\d+)?)S)?)?"
)


def seconds(value: str) -> timedelta:
    """Time given by an environment variable in seconds, also fractional (e.g. `0.2`)."""
    return timedelta(seconds=float(value))


def environment_variable(name: str, former_name: str | None = None) -> str | None:
    """Value of the environment variable - or of its former name (deprecated), when only that one is set."""
    if (value := environ.get(name)) is not None or former_name is None:
        return value
    if (value := environ.get(former_name)) is not None:
        warnings.warn(
            f"Environment variable `{former_name}` is deprecated, use `{name}` instead.",
            DeprecationWarning,
            stacklevel=2,
        )
    return value


@dataclass
class CommunicationSettings:
    """
    Settings of communication with services - retries and timeouts of requests.

    Defaults of members are read from environment variables (`EnvironNames`, or deprecated `FormerEnvironNames`) when
    the settings are created, not when the module is imported.
    """

    class EnvironNames:
        """Names of environment variables giving defaults of members of the settings."""

        TIMEOUT: ClassVar[str] = "HIVEIO_COMMUNICATION_TIMEOUT_SECS"
        PERIOD_BETWEEN_RETRIES: ClassVar[str] = "HIVEIO_COMMUNICATION_PERIOD_BETWEEN_RETRIES_SECS"
        RETRIES: ClassVar[str] = "HIVEIO_COMMUNICATION_MAX_RETRIES"

    class FormerEnvironNames:
        """Former names of environment variables - deprecated, read when variables of `EnvironNames` are not set."""

        TIMEOUT: ClassVar[str] = "HELPY_COMMUNICATION_TIMEOUT_SECS"
        PERIOD_BETWEEN_RETRIES: ClassVar[str] = "HELPY_COMMUNICATION_PERIOD_BETWEEN_RETRIES_SECS"
        RETRIES: ClassVar[str] = "HELPY_COMMUNICATION_MAX_RETRIES"

    class Defaults:
        TIMEOUT: ClassVar[timedelta] = timedelta(seconds=5)
        PERIOD_BETWEEN_RETRIES: ClassVar[timedelta] = timedelta(seconds=0.2)
        RETRIES: ClassVar[int] = 5

        @staticmethod
        def default_factory(
            env_name: str, default_factory: Callable[[str | None], T], *, former_env_name: str | None = None
        ) -> Callable[[], T]:
            """Factory of the default given by the environment variable - read when the settings are created."""
            return lambda: default_factory(environment_variable(env_name, former_env_name))

    max_retries: int = field(
        default_factory=Defaults.default_factory(
            EnvironNames.RETRIES,
            lambda x: CommunicationSettings.Defaults.RETRIES if x is None else int(x),
            former_env_name=FormerEnvironNames.RETRIES,
        )
    )
    """Amount of retries when sending request to service."""

    timeout: timedelta = field(
        default_factory=Defaults.default_factory(
            EnvironNames.TIMEOUT,
            lambda x: CommunicationSettings.Defaults.TIMEOUT if x is None else seconds(x),
            former_env_name=FormerEnvironNames.TIMEOUT,
        )
    )
    """Maximum time for single request to finish."""

    period_between_retries: timedelta = field(
        default_factory=Defaults.default_factory(
            EnvironNames.PERIOD_BETWEEN_RETRIES,
            lambda x: CommunicationSettings.Defaults.PERIOD_BETWEEN_RETRIES if x is None else seconds(x),
            former_env_name=FormerEnvironNames.PERIOD_BETWEEN_RETRIES,
        )
    )
    """Period between failed request and next retry."""

    unlimited_retries_timeout: timedelta | None = None
    """
    Maximum time of sending a request again while its responses request unlimited retries (e.g. while the node holds
    its database lock) - unlimited when `None`.
    """

    def copy(self) -> Self:
        """Deep copy of the settings."""
        return copy.deepcopy(self)

    def export_settings(self) -> str:
        """Settings as a JSON object - without members set to `None`, times are ISO 8601 durations (e.g. `PT0.2S`)."""
        exported = {member.name: getattr(self, member.name) for member in fields(self)}
        return json.dumps(
            {name: _exported(value) for name, value in exported.items() if value is not None}, ensure_ascii=False
        )

    @classmethod
    def import_settings(cls, settings: str) -> Self:
        """Settings exported by `export_settings` - members not given take their defaults."""
        from hiveio_api.client import HttpUrl

        from beekeepy._communication.abc.communicator import AbstractCommunicator

        # types of members imported for type checking only
        types = get_type_hints(cls, localns={"HttpUrl": HttpUrl, "AbstractCommunicator": AbstractCommunicator})
        imported = cls()
        for name, value in json.loads(settings).items():
            if name not in types:
                raise ValueError(f"Unknown member of `{cls.__name__}`: `{name}`")
            setattr(imported, name, _imported(value, types[name]))
        return imported


def _exported(value: object) -> object:
    from hiveio_api.client import Url

    if isinstance(value, timedelta):
        return _duration(value)
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, Url):
        return value.as_string()
    if isinstance(value, bool | int | float | str):
        return value
    raise TypeError(f"Settings with `{value!r}` cannot be exported")


def _imported(value: object, member_type: object) -> object:
    from hiveio_api.client import Url

    if value is None:
        return None
    types = get_args(member_type) if get_origin(member_type) in (Union, UnionType) else (member_type,)
    for type_ in types:
        if type_ is timedelta and isinstance(value, str):
            return _parsed_duration(value)
        if isinstance(type_, type) and issubclass(type_, Path | Url) and isinstance(value, str):
            return type_(value)
        if type_ in (bool, int, float, str) and type(value) is type_:
            return value
    raise ValueError(f"`{value!r}` is not a value of `{member_type}`")


def _duration(value: timedelta) -> str:
    """Time as an ISO 8601 duration, e.g. `PT0.2S`, `P1DT30S`."""
    sign, value = ("-", -value) if value < timedelta(0) else ("", value)
    seconds = f"{value.seconds}.{value.microseconds:06}".rstrip("0").rstrip(".")
    days = f"{value.days}D" if value.days else ""
    time = f"T{seconds}S" if seconds != "0" else ""
    return f"{sign}P{days}{time}" if days or time else "P0D"


def _parsed_duration(value: str) -> timedelta:
    if (match := _DURATION.fullmatch(value)) is None:
        raise ValueError(f"`{value}` is not an ISO 8601 duration")
    parts = {name: float(part) for name, part in match.groupdict().items() if name != "sign" and part is not None}
    duration = timedelta(**parts)
    return -duration if match["sign"] else duration
