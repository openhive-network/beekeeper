#pragma once

/// Standalone RIPEMD-160 hash — extracted from Bitcoin Core (MIT license).
/// No external dependencies.

#include <array>
#include <cstddef>
#include <cstdint>

namespace beekeeper_wasm {

std::array<uint8_t, 20> standalone_ripemd160(const uint8_t* data, size_t len);

} // namespace beekeeper_wasm
