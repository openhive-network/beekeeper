from __future__ import annotations

from typing import TYPE_CHECKING

from hiveio_api._utilities.smart_lazy_import import aggregate_same_import, lazy_module_factory

__all__ = [
    "Abort",
    "AbstractCommunicator",
    "AioHttpCommunicator",
    "async_is_url_reachable",
    "AsyncCallback",
    "AsyncCallbacks",
    "AsyncErrorCallback",
    "AsyncRequestCallback",
    "AsyncResponseCallback",
    "BeekeeperRules",
    "Callbacks",
    "CommunicationSettings",
    "ErrorCallback",
    "ErrorInResponseRules",
    "get_communicator_cls",
    "HiveRules",
    "JsonRpcRules",
    "Reaction",
    "Request",
    "RequestCallback",
    "RequestCommunicator",
    "RequestContext",
    "RequestTimings",
    "Response",
    "ResponseCallback",
    "response_id",
    "ResponsesRule",
    "Retry",
    "Rule",
    "rules",
    "RuleSet",
    "SchemaValidationRules",
    "sync_is_url_reachable",
    "SyncCallback",
]

if TYPE_CHECKING:
    from hiveio_api.client import (
        AsyncCallback,
        AsyncCallbacks,
        AsyncErrorCallback,
        AsyncRequestCallback,
        AsyncResponseCallback,
        Callbacks,
        ErrorCallback,
        Request,
        RequestCallback,
        RequestContext,
        RequestTimings,
        Response,
        ResponseCallback,
        SyncCallback,
    )

    from beekeepy._communication import rules
    from beekeepy._communication.abc.communicator import AbstractCommunicator
    from beekeepy._communication.abc.rules import Reaction, ResponsesRule, Rule, RuleSet, response_id
    from beekeepy._communication.aiohttp_communicator import AioHttpCommunicator
    from beekeepy._communication.communicator_getter import get_communicator_cls
    from beekeepy._communication.is_url_reachable import async_is_url_reachable, sync_is_url_reachable
    from beekeepy._communication.request_communicator import RequestCommunicator
    from beekeepy._communication.rules import (
        BeekeeperRules,
        ErrorInResponseRules,
        HiveRules,
        JsonRpcRules,
        SchemaValidationRules,
    )
    from beekeepy._communication.settings import CommunicationSettings
    from beekeepy._communication.signals import Abort, Retry


__getattr__ = lazy_module_factory(
    globals(),
    # Translations
    *aggregate_same_import(
        "async_is_url_reachable",
        "sync_is_url_reachable",
        module="beekeepy._communication.is_url_reachable",
    ),
    *aggregate_same_import(
        "AsyncCallback",
        "AsyncCallbacks",
        "AsyncErrorCallback",
        "AsyncRequestCallback",
        "AsyncResponseCallback",
        "Callbacks",
        "ErrorCallback",
        "Request",
        "RequestCallback",
        "RequestContext",
        "RequestTimings",
        "Response",
        "ResponseCallback",
        "SyncCallback",
        module="hiveio_api._communication.abc.communicator_models",
    ),
    *aggregate_same_import(
        "BeekeeperRules",
        "ErrorInResponseRules",
        "HiveRules",
        "JsonRpcRules",
        "SchemaValidationRules",
        module="beekeepy._communication.rules",
    ),
    *aggregate_same_import(
        "Reaction", "ResponsesRule", "Rule", "RuleSet", "response_id", module="beekeepy._communication.abc.rules"
    ),
    *aggregate_same_import("Abort", "Retry", module="beekeepy._communication.signals"),
    ("beekeepy._communication", "rules"),
    ("beekeepy._communication.abc.communicator", "AbstractCommunicator"),
    ("beekeepy._communication.settings", "CommunicationSettings"),
    ("beekeepy._communication.communicator_getter", "get_communicator_cls"),
    ("beekeepy._communication.aiohttp_communicator", "AioHttpCommunicator"),
    ("beekeepy._communication.request_communicator", "RequestCommunicator"),
)
