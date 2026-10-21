from __future__ import annotations

from dataclasses import dataclass

from hiveio_api import HiveModel

from schemas.fields.public import PublicKey  # noqa: TCH001  # resolved at runtime by get_type_hints


@dataclass(eq=False)
class KeyPair(HiveModel):
    public_key: PublicKey
    private_key: str

    def __hash__(self) -> int:
        return hash(self.private_key)

    def __eq__(self, value: object) -> bool:
        if isinstance(value, KeyPair):
            return self.private_key == value.private_key
        return super().__eq__(value)
