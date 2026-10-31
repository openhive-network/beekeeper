"""Lazy imports - moved to `hiveio_api._utilities.smart_lazy_import` (hiveio-api), its interface is re-exported here."""

from __future__ import annotations

from hiveio_api._utilities.smart_lazy_import import (
    aggregate_same_import,
    lazy_module_factory,
    smart_lazy_getattr,
)

__all__ = [
    "aggregate_same_import",
    "lazy_module_factory",
    "smart_lazy_getattr",
]
