from __future__ import annotations

import importlib

_SUBMODULES = frozenset({"rules"})
"""Submodules exported by `beekeepy.communication` - imported when accessed first."""


def __getattr__(name: str) -> object:
    if name in _SUBMODULES:
        return importlib.import_module(f"{__name__}.{name}")
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
