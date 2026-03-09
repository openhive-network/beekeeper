/// Standalone Base58 encoding/decoding — no OpenSSL, no bignum library.
/// Uses Bitcoin's standard alphabet: 123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz
/// Pure C implementation.

#ifndef STANDALONE_BASE58_H
#define STANDALONE_BASE58_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Encode `data` (length `data_len`) to Base58 into `out_buf`.
/// `out_buf_size` is the capacity of `out_buf`.
/// Returns the number of characters written (excluding NUL terminator),
/// or 0 if the buffer is too small.
/// The result is NUL-terminated if the buffer is large enough.
/// Safe upper bound for out_buf_size: data_len * 138 / 100 + 2
size_t standalone_base58_encode_c(const uint8_t* data, size_t data_len,
                                  char* out_buf, size_t out_buf_size);

/// Decode Base58 string `str` (length `str_len`) into `out_buf`.
/// `out_buf_size` is the capacity of `out_buf`.
/// Returns the number of bytes written, or 0 on error (invalid char or buffer too small).
/// Safe upper bound for out_buf_size: str_len * 733 / 1000 + 2
size_t standalone_base58_decode_c(const char* str, size_t str_len,
                                  uint8_t* out_buf, size_t out_buf_size);

#ifdef __cplusplus
}
#endif

#endif /* STANDALONE_BASE58_H */
