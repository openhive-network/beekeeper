from __future__ import annotations

from dataclasses import fields, is_dataclass
from typing import TYPE_CHECKING, get_args, get_origin, get_type_hints

import pytest
from beekeepy.settings import RemoteHandleSettings
from local_tools.beekeepy.simple_api import InputTypeSchema, TestCaller, WitnessesVotersResponseSchema

if TYPE_CHECKING:
    from hiveio_api.client import HttpUrl


@pytest.fixture
def caller(hived_http_endpoint: HttpUrl) -> TestCaller:
    """Fixture to create a TestCaller instance."""
    settings = RemoteHandleSettings(http_endpoint=hived_http_endpoint)
    return TestCaller(settings=settings, logger=None)


def validate_type(result: object, expected_type: object) -> None:
    """
    Check the type of the result - also of fields of models and items of lists, recursively.

    Models of responses are built without validation, so a field of a wrong type (or a missing one, set to None) is
    detected only here.
    """
    if get_origin(expected_type) is list:
        assert isinstance(result, list), f"Expected a list, but got: `{result}` of type `{type(result)}`"
        (item_type,) = get_args(expected_type)
        for item in result:
            validate_type(item, item_type)
        return

    assert isinstance(expected_type, type), f"Unsupported expected type: {expected_type}"
    assert isinstance(result, expected_type), (
        f"Expected result to be a {expected_type}, but got: `{result}` of type `{type(result)}`"
    )
    if is_dataclass(expected_type):
        hints = get_type_hints(expected_type)
        for field in fields(expected_type):
            validate_type(getattr(result, field.name), hints[field.name])


def test_no_args(caller: TestCaller) -> None:
    # ARRANGE & ACT
    result = caller.apis.test_api.last_synced_block()

    # ASSERT
    validate_type(result, int)


def test_pos_args(caller: TestCaller) -> None:
    # ARRANGE & ACT
    result = caller.apis.test_api.input_type("gtg")

    # ASSERT
    validate_type(result, InputTypeSchema)
    assert (result.input_type, result.input_value) == ("account_name", ["gtg"])


def test_pos_and_keyword_args(caller: TestCaller) -> None:
    # ARRANGE
    expected_amount_of_voters_1 = 10
    expected_amount_of_voters_2 = 5

    # ACT
    result_10_0 = caller.apis.test_api.witnesses_voters("blocktrades", page_size=10)
    result_5_0 = caller.apis.test_api.witnesses_voters("blocktrades", page_size=5)
    result_5_2 = caller.apis.test_api.witnesses_voters("blocktrades", page_size=5, page=2)

    # ASSERT
    validate_type(result_10_0, WitnessesVotersResponseSchema)
    validate_type(result_5_0, WitnessesVotersResponseSchema)
    validate_type(result_5_2, WitnessesVotersResponseSchema)

    assert len(result_10_0.voters) == expected_amount_of_voters_1, "Expected 10 voters in the response"
    assert len(result_5_0.voters) == expected_amount_of_voters_2, "Expected 5 voters in the response"
    assert len(result_5_2.voters) == expected_amount_of_voters_2, "Expected 5 voters in the response for page 2"

    assert result_5_2.voters != result_5_0.voters, "Expected different voters for different pages"
