/// Standalone Base58 encoding/decoding using manual big-integer arithmetic.
/// No OpenSSL, no GMP — just byte-vector divmod.
///
/// Algorithm matches Bitcoin's standard Base58 (used by FC/Hive):
/// - Alphabet: 123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz
/// - Leading zero bytes encode as '1'

#include "standalone_base58.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

constexpr char ALPHABET[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Reverse lookup table: char → digit value (255 = invalid)
constexpr uint8_t make_reverse_table(char c)
{
  for (int i = 0; i < 58; ++i)
    if (ALPHABET[i] == c)
      return static_cast<uint8_t>(i);
  return 255;
}

// Build at compile time
constexpr std::array<uint8_t, 128> build_reverse_table()
{
  std::array<uint8_t, 128> table{};
  for (int i = 0; i < 128; ++i)
    table[i] = 255;
  for (int i = 0; i < 58; ++i)
    table[static_cast<int>(ALPHABET[i])] = static_cast<uint8_t>(i);
  return table;
}

constexpr auto REVERSE = build_reverse_table();

} // anonymous namespace

namespace beekeeper_wasm {

std::string standalone_base58_encode(const uint8_t* data, size_t len)
{
  // Count leading zeros → each encodes as '1'
  size_t zeros = 0;
  while (zeros < len && data[zeros] == 0)
    ++zeros;

  // Work on a mutable copy of the non-zero portion
  std::vector<uint8_t> num(data + zeros, data + len);

  // Repeatedly divide by 58 to extract digits (least significant first)
  std::string result;
  result.reserve(len * 138 / 100 + 1); // log(256)/log(58) ≈ 1.366

  while (!num.empty())
  {
    uint32_t carry = 0;
    size_t write_idx = 0;

    for (size_t i = 0; i < num.size(); ++i)
    {
      uint32_t val = carry * 256 + num[i];
      num[write_idx] = static_cast<uint8_t>(val / 58);
      carry = val % 58;
      if (num[write_idx] != 0 || write_idx > 0)
        ++write_idx;
    }
    num.resize(write_idx);

    result += ALPHABET[carry];
  }

  // Add leading '1's
  result.append(zeros, '1');

  // Reverse — we built least-significant-first
  std::reverse(result.begin(), result.end());
  return result;
}

std::vector<uint8_t> standalone_base58_decode(const std::string& str)
{
  // Count leading '1's → zero bytes
  size_t zeros = 0;
  while (zeros < str.size() && str[zeros] == '1')
    ++zeros;

  // Work in base-58 digits, converting to base-256
  std::vector<uint8_t> num;
  num.reserve(str.size() * 733 / 1000 + 1); // log(58)/log(256) ≈ 0.733

  for (size_t i = zeros; i < str.size(); ++i)
  {
    unsigned char ch = static_cast<unsigned char>(str[i]);
    if (ch >= 128)
      throw std::runtime_error("Invalid Base58 character");

    uint8_t digit = REVERSE[ch];
    if (digit == 255)
      throw std::runtime_error("Invalid Base58 character");

    // Multiply num by 58 and add digit
    uint32_t carry = digit;
    for (auto it = num.rbegin(); it != num.rend(); ++it)
    {
      uint32_t val = static_cast<uint32_t>(*it) * 58 + carry;
      *it = static_cast<uint8_t>(val & 0xFF);
      carry = val >> 8;
    }
    while (carry > 0)
    {
      num.insert(num.begin(), static_cast<uint8_t>(carry & 0xFF));
      carry >>= 8;
    }
  }

  // Prepend zero bytes
  std::vector<uint8_t> result(zeros, 0);
  result.insert(result.end(), num.begin(), num.end());
  return result;
}

} // namespace beekeeper_wasm
