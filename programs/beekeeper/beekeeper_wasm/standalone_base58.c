/// Standalone Base58 encoding/decoding using manual big-integer arithmetic.
/// No OpenSSL, no GMP — just byte-array divmod.
/// Pure C implementation.
///
/// Algorithm matches Bitcoin's standard Base58 (used by FC/Hive):
/// - Alphabet: 123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz
/// - Leading zero bytes encode as '1'

#include "standalone_base58.h"

#include <string.h>

static const char ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static const uint8_t REVERSE[128] = {
  255,255,255,255,255,255,255,255, 255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255, 255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255, 255,255,255,255,255,255,255,255,
  255,  0,  1,  2,  3,  4,  5,  6,   7,  8,255,255,255,255,255,255, /* '1'..'9' = 0..8 */
  255,  9, 10, 11, 12, 13, 14, 15,  16,255, 17, 18, 19, 20, 21,255, /* A..O (no I) */
   22, 23, 24, 25, 26, 27, 28, 29,  30, 31, 32,255,255,255,255,255, /* P..Z */
  255, 33, 34, 35, 36, 37, 38, 39,  40, 41, 42, 43,255, 44, 45, 46, /* a..o (no l) */
   47, 48, 49, 50, 51, 52, 53, 54,  55, 56, 57,255,255,255,255,255  /* p..z */
};

size_t standalone_base58_encode_c(const uint8_t* data, size_t data_len,
                                  char* out_buf, size_t out_buf_size)
{
  size_t zeros = 0;
  size_t num_len;
  uint8_t num_buf[512];
  size_t pos;
  size_t i;
  size_t result_len;

  if (out_buf_size == 0)
    return 0;

  /* Count leading zeros */
  while (zeros < data_len && data[zeros] == 0)
    ++zeros;

  num_len = data_len - zeros;
  if (num_len > sizeof(num_buf))
    return 0; /* input too large for stack buffer */

  memcpy(num_buf, data + zeros, num_len);

  /* Write digits from the end of out_buf, working backward.
     Division extracts least-significant digit first, which becomes
     the rightmost character — so writing backward produces correct order. */
  pos = out_buf_size;

  while (num_len > 0)
  {
    uint32_t carry = 0;
    size_t write_idx = 0;

    for (i = 0; i < num_len; ++i)
    {
      uint32_t val = carry * 256 + num_buf[i];
      num_buf[write_idx] = (uint8_t)(val / 58);
      carry = val % 58;
      if (num_buf[write_idx] != 0 || write_idx > 0)
        ++write_idx;
    }
    num_len = write_idx;

    if (pos == 0)
      return 0; /* buffer too small */
    out_buf[--pos] = ALPHABET[carry];
  }

  /* Prepend leading '1's */
  for (i = 0; i < zeros; ++i)
  {
    if (pos == 0)
      return 0;
    out_buf[--pos] = '1';
  }

  /* Move result to front of buffer */
  result_len = out_buf_size - pos;
  if (result_len + 1 > out_buf_size)
    return 0; /* no room for NUL */

  memmove(out_buf, out_buf + pos, result_len);
  out_buf[result_len] = '\0';
  return result_len;
}

size_t standalone_base58_decode_c(const char* str, size_t str_len,
                                  uint8_t* out_buf, size_t out_buf_size,
                                 void (*error_handler)(const char* error))
{
  size_t zeros = 0;
  uint8_t num_buf[512];
  size_t num_start; /* index of first used byte in num_buf */
  size_t num_len;
  size_t i, result_len;

  /* Count leading '1's → zero bytes */
  while (zeros < str_len && str[zeros] == '1')
    ++zeros;

  /* Build number in num_buf, growing downward from the end.
     This avoids memmove on every carry overflow. */
  num_start = sizeof(num_buf);

  for (i = zeros; i < str_len; ++i)
  {
    unsigned char ch = (unsigned char)str[i];
    uint8_t digit;
    uint32_t carry;
    size_t j;

    if (ch >= 128) {
      error_handler("Invalid Base58 character");
      return 0; /* invalid character */
    }

    digit = REVERSE[ch];
    if (digit == 255) {
      error_handler("Invalid Base58 character");
      return 0; /* invalid character */
    }

    /* Multiply existing number by 58 and add digit */
    carry = digit;
    for (j = sizeof(num_buf); j > num_start; )
    {
      --j;
      carry += (uint32_t)num_buf[j] * 58;
      num_buf[j] = (uint8_t)(carry & 0xFF);
      carry >>= 8;
    }
    while (carry > 0)
    {
      if (num_start == 0)
        return 0; /* overflow */
      --num_start;
      num_buf[num_start] = (uint8_t)(carry & 0xFF);
      carry >>= 8;
    }
  }

  /* Total result = zeros + num bytes */
  num_len = sizeof(num_buf) - num_start;
  result_len = zeros + num_len;
  if (result_len > out_buf_size)
    return 0;

  memset(out_buf, 0, zeros);
  memcpy(out_buf + zeros, num_buf + num_start, num_len);
  return result_len;
}
