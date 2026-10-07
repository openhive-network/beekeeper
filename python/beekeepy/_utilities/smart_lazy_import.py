"""Moved to `hiveio_api._utilities.smart_lazy_import` (hiveio-api).

Re-exported here for backward compatibility.
"""

from __future__ import annotations

from hiveio_api._utilities.smart_lazy_import import (
    AggregatedAliasedImportInput,
    AggregatedModuleInterfaceMappingInput,
    Alias,
    AliasedImportInput,
    AliasMapping,
    GetattrProtocol,
    ImportInput,
    ModuleComponent,
    ModuleGlobals,
    ModuleInterfaceMapping,
    ModuleInterfaceMappingInput,
    ModulePath,
    _extract_aliases,
    _validate_all_in_translations,
    aggregate_same_import,
    lazy_module_factory,
    smart_lazy_getattr,
)

__all__ = [
    "AggregatedAliasedImportInput",
    "AggregatedModuleInterfaceMappingInput",
    "Alias",
    "AliasedImportInput",
    "AliasMapping",
    "GetattrProtocol",
    "ImportInput",
    "ModuleComponent",
    "ModuleGlobals",
    "ModuleInterfaceMapping",
    "ModuleInterfaceMappingInput",
    "ModulePath",
    "_extract_aliases",
    "_validate_all_in_translations",
    "aggregate_same_import",
    "lazy_module_factory",
    "smart_lazy_getattr",
]
