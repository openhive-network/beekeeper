from __future__ import annotations

import json
from dataclasses import replace
from typing import TYPE_CHECKING, Final

import pytest
from beekeepy.communication import Callbacks, CommunicationSettings, get_communicator_cls
from beekeepy.interfaces import HttpUrl
from requests.status_codes import codes

if TYPE_CHECKING:
    from beekeepy.communication import Request, RequestContext


@pytest.fixture
def simple_get(hived_http_endpoint: HttpUrl) -> HttpUrl:
    return HttpUrl(
        hived_http_endpoint.address,
        port=hived_http_endpoint.port,
        path="hafbe-api/accounts/gtg",
        protocol=hived_http_endpoint.protocol,
    )


@pytest.fixture
def invalid_get(hived_http_endpoint: HttpUrl) -> HttpUrl:
    return HttpUrl(
        hived_http_endpoint.address,
        port=hived_http_endpoint.port,
        path="hafbe-api/acnts/gtg",
        protocol=hived_http_endpoint.protocol,
    )


def test_simple_restapi(simple_get: HttpUrl) -> None:
    # ARRANGE
    communicator = get_communicator_cls("sync")(settings=CommunicationSettings())

    # ACT
    result = communicator.get(url=simple_get)

    # ASSERT
    assert isinstance(result, str)
    assert len(result) > 0


def test_restapi_with_error(invalid_get: HttpUrl) -> None:
    # ARRANGE
    communicator = get_communicator_cls("sync")(settings=CommunicationSettings())

    # ACT
    result = communicator.get(url=invalid_get)

    # ASSERT
    assert isinstance(result, str)
    assert len(result) > 0


def test_restapi_with_error_and_callback_reraise(invalid_get: HttpUrl) -> None:
    # ARRANGE
    error_message: Final[str] = "Invalid status code received"
    communicator = get_communicator_cls("sync")(settings=CommunicationSettings())

    def raise_if_invalid_status_code(context: RequestContext, /) -> None:
        assert context.response is not None
        if context.response.status_code != codes.ok:
            raise ValueError(error_message)

    # ACT & ASSERT
    with pytest.raises(ValueError, match=error_message):
        communicator.get(
            url=invalid_get,
            callbacks=Callbacks(process_response=[raise_if_invalid_status_code]),
        )


def test_restapi_with_request_correction(simple_get: HttpUrl, invalid_get: HttpUrl) -> None:
    # ARRANGE
    communicator = get_communicator_cls("sync")(settings=CommunicationSettings())

    statuses: list[int] = []

    def fix_url(request: Request, /) -> Request:
        return replace(request, url=simple_get)

    def record_status(context: RequestContext, /) -> None:
        assert context.response is not None
        statuses.append(context.response.status_code)

    # ACT
    result = communicator.get(
        url=invalid_get,
        callbacks=Callbacks(prepare_request=[fix_url], process_response=[record_status]),
    )

    # ASSERT
    assert statuses == [codes.ok], "the corrected request is sent"
    assert json.loads(result)["name"] == "gtg"
