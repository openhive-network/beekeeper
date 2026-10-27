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

/* Both directions accumulate the converted number least-significant digit
   first directly in out_buf, so the input size is bounded only by the
   caller's buffer, then reverse it into big-endian order. */

static void reverse_bytes(uint8_t* buf, size_t len)
{
  size_t i;
  for (i = 0; i < len / 2; ++i)
  {
    uint8_t tmp = buf[i];
    buf[i] = buf[len - 1 - i];
    buf[len - 1 - i] = tmp;
  }
}

size_t standalone_base58_encode_c(const uint8_t* data, size_t data_len,
                                  char* out_buf, size_t out_buf_size)
{
  uint8_t* digits = (uint8_t*)out_buf;
  size_t zeros = 0;
  size_t digits_len = 0;
  size_t i, j;
  size_t result_len;

  /* Count leading zeros */
  while (zeros < data_len && data[zeros] == 0)
    ++zeros;

  for (i = zeros; i < data_len; ++i)
  {
    /* Multiply existing number by 256 and add the next byte */
    uint32_t carry = data[i];
    for (j = 0; j < digits_len; ++j)
    {
      carry += (uint32_t)digits[j] << 8;
      digits[j] = (uint8_t)(carry % 58);
      carry /= 58;
    }
    while (carry > 0)
    {
      if (digits_len >= out_buf_size)
        return 0; /* buffer too small */
      digits[digits_len++] = (uint8_t)(carry % 58);
      carry /= 58;
    }
  }

  result_len = zeros + digits_len;
  if (result_len + 1 > out_buf_size)
    return 0; /* no room for leading '1's and NUL */

  reverse_bytes(digits, digits_len);
  for (i = 0; i < digits_len; ++i)
    out_buf[i] = ALPHABET[digits[i]];

  memmove(out_buf + zeros, out_buf, digits_len);
  memset(out_buf, '1', zeros);
  out_buf[result_len] = '\0';
  return result_len;
}

size_t standalone_base58_decode_c(const char* str, size_t str_len,
                                  uint8_t* out_buf, size_t out_buf_size,
                                 void (*error_handler)(const char* error))
{
  size_t zeros = 0;
  size_t num_len = 0;
  size_t i, j, result_len;

  /* Count leading '1's → zero bytes */
  while (zeros < str_len && str[zeros] == '1')
    ++zeros;

  for (i = zeros; i < str_len; ++i)
  {
    unsigned char ch = (unsigned char)str[i];
    uint8_t digit;
    uint32_t carry;

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
    for (j = 0; j < num_len; ++j)
    {
      carry += (uint32_t)out_buf[j] * 58;
      out_buf[j] = (uint8_t)(carry & 0xFF);
      carry >>= 8;
    }
    while (carry > 0)
    {
      if (num_len >= out_buf_size)
        return 0; /* buffer too small */
      out_buf[num_len++] = (uint8_t)(carry & 0xFF);
      carry >>= 8;
    }
  }

  /* Total result = zeros + num bytes */
  result_len = zeros + num_len;
  if (result_len > out_buf_size)
    return 0;

  reverse_bytes(out_buf, num_len);
  memmove(out_buf + zeros, out_buf, num_len);
  memset(out_buf, 0, zeros);
  return result_len;
}
