// Copyright (c) 2014-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Standalone RIPEMD-160 hash — extracted from Bitcoin Core (MIT license).
// Pure C implementation, no external dependencies.

#ifndef STANDALONE_RIPEMD160_H
#define STANDALONE_RIPEMD160_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Compute RIPEMD-160 hash of `data` (length `len` bytes).
/// Result is written to `out`, which must point to at least 20 bytes.
void standalone_ripemd160(const uint8_t* data, size_t len, uint8_t out[20]);

#ifdef __cplusplus
}
#endif

#endif // STANDALONE_RIPEMD160_H
