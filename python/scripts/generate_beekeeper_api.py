# ruff: noqa: INP001
"""
Generate models of beekeeper_api from the OpenAPI definition of beekeeper.

Two model sets are generated from programs/beekeeper/beekeeper/documentation/api/openapi.json, with the `x-hive-type`
annotations mapped as in hiveio_api (`api_client_generator.hive_variants`):

- public models (beekeepy/_generated/beekeeper_api_description.py) - dataclasses
  (`hiveio_api.HiveModel`) with builtin types named by the aliases of `schemas.fields.public` -
  returned by API calls, never validated,
- validation models (tests/local-tools/local_tools/beekeepy/beekeeper_api_validation.py) - msgspec models
  (`PreconfiguredBaseModel`) with validation forms of the types (`schemas.fields.validation`) and
  `ENDPOINT_RESULTS`, used by `hiveio_api.validate_schema` (`SchemaValidationRules`) in tests of beekeepy only - msgspec
  is a dependency of the tests (the `dev` group), not of beekeepy.

The generated modules are committed - CI regenerates them and fails when they differ.

Usage (poetry environment with the `generation` group): python scripts/generate_beekeeper_api.py
"""

from __future__ import annotations

from pathlib import Path
from typing import Final

from api_client_generator import hive_variants
from api_client_generator.json_rpc import append_endpoint_results, generate_api_description
from api_client_generator.tidy import tidy_with_ruff

from schemas.fields import public, validation

REPOSITORY_DIRECTORY: Final[Path] = Path(__file__).resolve().parents[2]
OPENAPI_DEFINITION: Final[Path] = REPOSITORY_DIRECTORY / "programs/beekeeper/beekeeper/documentation/api/openapi.json"
OUTPUT_DIRECTORY: Final[Path] = REPOSITORY_DIRECTORY / "python/beekeepy/_generated"
PUBLIC_MODULE: Final[Path] = OUTPUT_DIRECTORY / "beekeeper_api_description.py"
VALIDATION_MODULE: Final[Path] = (
    REPOSITORY_DIRECTORY / "python/tests/local-tools/local_tools/beekeepy/beekeeper_api_validation.py"
)
DESCRIPTION_NAME: Final[str] = "beekeeper_api_description"


def main() -> None:
    for output, model_options, custom_types in (
        (PUBLIC_MODULE, hive_variants.public_model_options(), hive_variants.public_custom_types(public.__all__)),
        (
            VALIDATION_MODULE,
            hive_variants.VALIDATION_MODEL_OPTIONS,
            hive_variants.validation_custom_types(validation.__all__),
        ),
    ):
        generate_api_description(
            DESCRIPTION_NAME,
            OPENAPI_DEFINITION,
            output,
            allow_passing_item_suffix=False,
            model_options=model_options,
            custom_types=custom_types,
            flatten=True,
        )
    append_endpoint_results(VALIDATION_MODULE, DESCRIPTION_NAME)
    tidy_with_ruff(PUBLIC_MODULE, VALIDATION_MODULE)


if __name__ == "__main__":
    main()
