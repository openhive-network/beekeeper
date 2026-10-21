from __future__ import annotations

import inspect
from dataclasses import fields, is_dataclass
from typing import TYPE_CHECKING, Final

import pytest
from beekeepy._generated.beekeeper_api_description import beekeeper_api_description
from beekeepy.communication import SchemaValidationRules
from beekeepy.exceptions import SchemaValidationError
from beekeepy.handle.remote import (
    AsyncBeekeeper,
    AsyncBeekeeperApi,
    Beekeeper,
    RemoteHandleSettings,
    SyncBeekeeperApi,
)
from beekeepy.interfaces import HttpUrl
from hiveio_api import validate_schema
from local_tools.beekeepy.beekeeper_api_validation import ENDPOINT_RESULTS
from local_tools.beekeepy.testing_server import run_simple_server

if TYPE_CHECKING:
    from beekeepy.handle.remote import RegisteredApisT

API_NAME: Final[str] = "beekeeper_api"
DESCRIPTION: Final = beekeeper_api_description[API_NAME]
INFO_NOT_MATCHING_SCHEMA: Final[str] = """{"now": 1, "timeout_time": "2026-01-01T00:00:00"}"""
"""Result of `get_info` with a number instead of the time."""


def parameters_of_endpoint(api: type[SyncBeekeeperApi | AsyncBeekeeperApi], endpoint: str) -> set[str]:
    return set(inspect.signature(getattr(api, endpoint)).parameters) - {"self"}


def parameters_in_description(endpoint: str) -> set[str]:
    description = DESCRIPTION[endpoint]
    assert isinstance(description, dict)
    params = description["params"]
    if params is None:
        return set()
    assert isinstance(params, type)
    assert is_dataclass(params)
    return {field.name for field in fields(params)}


@pytest.mark.parametrize("synchronous", [True, False], ids=["sync", "async"])
def test_endpoints_of_api_are_described(registered_apis: RegisteredApisT, synchronous: bool) -> None:
    # ARRANGE
    api = SyncBeekeeperApi if synchronous else AsyncBeekeeperApi

    # ACT
    endpoints = registered_apis[synchronous][API_NAME]

    # ASSERT
    assert endpoints == set(DESCRIPTION)
    assert {endpoint: parameters_of_endpoint(api, endpoint) for endpoint in endpoints} == {
        endpoint: parameters_in_description(endpoint) for endpoint in endpoints
    }


def test_endpoints_of_api_have_validation_models(registered_apis: RegisteredApisT) -> None:
    # ASSERT
    assert set(ENDPOINT_RESULTS) == registered_apis[True][API_NAME] == registered_apis[False][API_NAME]


def validating_beekeeper(url: HttpUrl) -> Beekeeper:
    handle = Beekeeper(settings=RemoteHandleSettings(http_endpoint=url, max_retries=0))
    handle.apis.callbacks.process_response.append(SchemaValidationRules(strict=True))
    handle.set_session_token("token")
    return handle


def test_response_not_matching_its_validation_model_is_rejected() -> None:
    # ARRANGE
    response = f"""{{"jsonrpc": "2.0", "result": {INFO_NOT_MATCHING_SCHEMA}, "id": 0}}"""

    # ACT
    with run_simple_server(response) as url:
        handle = validating_beekeeper(url)
        try:
            with pytest.raises(SchemaValidationError) as error:
                handle.api.get_info()
        finally:
            handle.teardown()

    # ASSERT
    assert [schema_error.path for schema_error in error.value.schema_errors] == ["$.now"]


def test_response_of_batch_not_matching_its_validation_model_is_rejected() -> None:
    # ARRANGE
    response = f"""[{{"jsonrpc": "2.0", "result": {INFO_NOT_MATCHING_SCHEMA}, "id": 0}}]"""

    # ACT
    with run_simple_server(response) as url:
        handle = validating_beekeeper(url)
        try:
            with pytest.raises(SchemaValidationError) as error, handle.batch() as batch:
                batch.api.beekeeper.get_info()
        finally:
            handle.teardown()

    # ASSERT
    assert [schema_error.path for schema_error in error.value.schema_errors] == ["$.now"]


def test_responses_of_app_status_api_are_validated_with_its_validation_models() -> None:
    # ARRANGE
    handle = Beekeeper(settings=RemoteHandleSettings(http_endpoint=HttpUrl("127.0.0.1:1")))

    # ACT
    try:
        schema_errors = validate_schema({}, handle.apis.app_status.get_app_status)
    finally:
        handle.teardown()

    # ASSERT
    assert schema_errors, "the empty result misses all members of the validation model"


async def test_responses_of_app_status_api_of_async_handle_are_validated_with_its_validation_models() -> None:
    # ARRANGE
    handle = AsyncBeekeeper(settings=RemoteHandleSettings(http_endpoint=HttpUrl("127.0.0.1:1")))

    # ACT
    try:
        schema_errors = validate_schema({}, handle.apis.app_status.get_app_status)
    finally:
        await handle.async_teardown()

    # ASSERT
    assert schema_errors, "the empty result misses all members of the validation model"
