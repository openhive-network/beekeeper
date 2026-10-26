from __future__ import annotations

import uuid
from typing import NoReturn

__all__ = [
    "handle_target_service_name",
    "raise_acquire_not_possible",
    "random_string",
]

handle_target_service_name = "beekeeper"


def random_string() -> str:
    return str(uuid.uuid4())


def raise_acquire_not_possible() -> NoReturn:
    raise RuntimeError("Batch handle has predefined token and should not create its own")
