from __future__ import annotations

import sys
import types
from typing import TYPE_CHECKING, Final

import pytest
from beekeepy.communication import (
    CommonOverseer,
    CommunicationSettings,
    StrictOverseer,
    get_communicator_cls,
)
from beekeepy.exceptions import (
    ApiNotFoundError,
    JussiResponseError,
    NullResultError,
    OverseerError,
    SchemaValidationError,
    UnparsableResponseError,
)
from local_tools.beekeepy.testing_server import run_simple_server

from schemas._preconfigured_base_model import PreconfiguredBaseModel
from schemas.fields.basic import AccountName  # noqa: TCH001  # resolved at runtime by msgspec
from schemas.validation import register_validation_models

if TYPE_CHECKING:
    from collections.abc import Iterator

    from beekeepy.communication import AbstractCommunicator, AbstractOverseer


ERRORS_TO_DETECT: Final[list[tuple[type[OverseerError], str]]] = [
    (
        NullResultError,
        """{"jsonrpc": "2.0", "result": null, "id": 1}""",
    ),
    (
        ApiNotFoundError,
        """{"jsonrpc": "2.0", "error": {"code": -32003, "message":
        "Assert Exception:api_itr != data._registered_apis.end(): Could not find API debug_node_api"
        }, "id": 1}""",
    ),
    (
        JussiResponseError,
        """{"jsonrpc":"2.0","id":null,"error":{"code":-32603,"message":
        "Internal Error","data":{"error_id":"b6384d8c-95ad-4af0-92dc-dd7828d3c707",
        "jussi_request_id":"000312363819934224"}}}""",
    ),
    (UnparsableResponseError, """404: Not Found"""),
]

SYNC_COMMUNICATORS: Final[list[type[AbstractCommunicator]]] = [
    get_communicator_cls("sync"),
]
ASYNC_COMMUNICATORS: Final[list[type[AbstractCommunicator]]] = [
    get_communicator_cls("async"),
]
OVERSEERS: Final[list[type[AbstractOverseer]]] = [CommonOverseer, StrictOverseer]

REQUEST: Final[str] = """{"method": "aaa", "id": 1, "jsonrpc": "2.0"}"""


@pytest.mark.parametrize("error_and_message", ERRORS_TO_DETECT)
@pytest.mark.parametrize("overseer_cls", OVERSEERS)
@pytest.mark.parametrize("communicator", SYNC_COMMUNICATORS)
def test_sync_overseer(
    error_and_message: tuple[type[OverseerError], str],
    overseer_cls: type[AbstractOverseer],
    communicator: type[AbstractCommunicator],
) -> None:
    error, message = error_and_message
    overseer = overseer_cls(communicator=communicator(settings=CommunicationSettings()))
    try:
        with run_simple_server(message) as url, pytest.raises(error):
            overseer.send(url=url, method="POST", data=REQUEST)
    finally:
        overseer.teardown()


@pytest.mark.parametrize("error_and_message", ERRORS_TO_DETECT)
@pytest.mark.parametrize("overseer_cls", OVERSEERS)
@pytest.mark.parametrize("communicator", ASYNC_COMMUNICATORS)
async def test_async_overseer(
    error_and_message: tuple[type[OverseerError], str],
    overseer_cls: type[AbstractOverseer],
    communicator: type[AbstractCommunicator],
) -> None:
    error, message = error_and_message
    overseer = overseer_cls(communicator=communicator(settings=CommunicationSettings()))
    try:
        with run_simple_server(message) as url, pytest.raises(error):
            await overseer.async_send(url=url, method="POST", data=REQUEST)
    finally:
        overseer.teardown()


SCHEMA_VALIDATION_MODULE: Final[str] = "beekeepy_test_fake_validation_models"
SCHEMA_REQUEST: Final[str] = """{"method": "fake_api.get_account", "id": 1, "jsonrpc": "2.0"}"""


class FakeAccount(PreconfiguredBaseModel):
    name: AccountName


@pytest.fixture
def fake_validation_models() -> Iterator[None]:
    module = types.ModuleType(SCHEMA_VALIDATION_MODULE)
    module.ENDPOINT_RESULTS = {"get_account": (FakeAccount, False)}  # type: ignore[attr-defined]
    sys.modules[SCHEMA_VALIDATION_MODULE] = module
    register_validation_models("fake_api", SCHEMA_VALIDATION_MODULE)
    yield
    del sys.modules[SCHEMA_VALIDATION_MODULE]


@pytest.mark.usefixtures("fake_validation_models")
@pytest.mark.parametrize("communicator", SYNC_COMMUNICATORS)
def test_strict_overseer_detects_response_not_matching_schema(communicator: type[AbstractCommunicator]) -> None:
    overseer = StrictOverseer(communicator=communicator(settings=CommunicationSettings()))
    try:
        with (
            run_simple_server("""{"jsonrpc": "2.0", "result": {"name": "Invalid Name!"}, "id": 1}""") as url,
            pytest.raises(SchemaValidationError) as error,
        ):
            overseer.send(url=url, method="POST", data=SCHEMA_REQUEST)
    finally:
        overseer.teardown()

    assert [schema_error.path for schema_error in error.value.schema_errors] == ["$.name"]


@pytest.mark.usefixtures("fake_validation_models")
@pytest.mark.parametrize(
    ("overseer_cls", "request_", "response"),
    [
        (StrictOverseer, SCHEMA_REQUEST, """{"jsonrpc": "2.0", "result": {"name": "alice"}, "id": 1}"""),
        (StrictOverseer, REQUEST, """{"jsonrpc": "2.0", "result": {"name": "Invalid Name!"}, "id": 1}"""),
        (CommonOverseer, SCHEMA_REQUEST, """{"jsonrpc": "2.0", "result": {"name": "Invalid Name!"}, "id": 1}"""),
    ],
    ids=["matching-schema", "endpoint-without-schema", "common-overseer-does-not-validate"],
)
@pytest.mark.parametrize("communicator", SYNC_COMMUNICATORS)
def test_schema_is_not_validated_or_matches(
    overseer_cls: type[AbstractOverseer],
    request_: str,
    response: str,
    communicator: type[AbstractCommunicator],
) -> None:
    overseer = overseer_cls(communicator=communicator(settings=CommunicationSettings()))
    try:
        with run_simple_server(response) as url:
            assert overseer.send(url=url, method="POST", data=request_)["result"]  # type: ignore[call-overload]
    finally:
        overseer.teardown()
