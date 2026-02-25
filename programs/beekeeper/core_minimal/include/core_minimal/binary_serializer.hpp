#pragma once

/// fc::raw-compatible binary serializer for wallet data.
/// Produces identical byte sequences to fc::raw::pack_to_vector / unpack_from_vector
/// for the specific types used in wallet encryption.
///
/// Format rules (matching FC):
///   uint8_t  : 1 byte
///   uint32_t : 4 bytes little-endian
///   uint64_t : 8 bytes little-endian
///   byte_array<N> : N raw bytes
///   std::string : varint(length) + UTF-8 bytes
///   std::vector<char> : varint(length) + raw bytes
///   std::pair<A,B> : pack(A) + pack(B)
///   std::map<K,V> : varint(size) + (pack(key) + pack(value)) * size

#include <core_minimal/crypto_types.hpp>

#include <cstdint>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace beekeeper_minimal {

// ── Packing (serialize) ────────────────────────────────────────

class packer
{
public:
  std::vector<char>& buf;

  explicit packer(std::vector<char>& b) : buf(b) {}

  void write(const void* src, size_t len)
  {
    auto p = static_cast<const char*>(src);
    buf.insert(buf.end(), p, p + len);
  }

  void pack_varint(uint32_t val)
  {
    while (val >= 0x80)
    {
      buf.push_back(static_cast<char>((val & 0x7F) | 0x80));
      val >>= 7;
    }
    buf.push_back(static_cast<char>(val));
  }

  void pack_uint8(uint8_t v) { buf.push_back(static_cast<char>(v)); }

  void pack_uint32(uint32_t v)
  {
    // little-endian
    char tmp[4];
    std::memcpy(tmp, &v, 4);
    write(tmp, 4);
  }

  void pack_uint64(uint64_t v)
  {
    // little-endian
    char tmp[8];
    std::memcpy(tmp, &v, 8);
    write(tmp, 8);
  }

  template<size_t N>
  void pack_bytes(const byte_array<N>& a)
  {
    write(a.data.data(), N);
  }

  void pack_string(const std::string& s)
  {
    pack_varint(static_cast<uint32_t>(s.size()));
    if (!s.empty())
      write(s.data(), s.size());
  }

  void pack_vector_char(const std::vector<char>& v)
  {
    pack_varint(static_cast<uint32_t>(v.size()));
    if (!v.empty())
      write(v.data(), v.size());
  }

  void pack_key_data(const key_data& kd)
  {
    pack_bytes(kd.first);   // private_key_type (32 bytes)
    pack_string(kd.second); // prefix string
  }

  void pack_keys_map(const keys_map& km)
  {
    pack_varint(static_cast<uint32_t>(km.size()));
    for (auto& [pub, data] : km)
    {
      pack_bytes(pub);       // public_key_type (33 bytes)
      pack_key_data(data);   // pair<private_key_type, string>
    }
  }
};

// ── Unpacking (deserialize) ────────────────────────────────────

class unpacker
{
public:
  const char* ptr;
  const char* end;

  unpacker(const char* data, size_t len) : ptr(data), end(data + len) {}
  unpacker(const std::vector<char>& v) : ptr(v.data()), end(v.data() + v.size()) {}

  void check(size_t n) const
  {
    if (static_cast<size_t>(end - ptr) < n)
      throw std::runtime_error("binary_serializer: unexpected end of data");
  }

  void read(void* dst, size_t len)
  {
    check(len);
    std::memcpy(dst, ptr, len);
    ptr += len;
  }

  uint32_t unpack_varint()
  {
    uint32_t val = 0;
    unsigned shift = 0;
    for (;;)
    {
      check(1);
      auto b = static_cast<uint8_t>(*ptr++);
      val |= static_cast<uint32_t>(b & 0x7F) << shift;
      if (!(b & 0x80))
        break;
      shift += 7;
      if (shift >= 35)
        throw std::runtime_error("binary_serializer: varint overflow");
    }
    return val;
  }

  uint8_t unpack_uint8()
  {
    check(1);
    return static_cast<uint8_t>(*ptr++);
  }

  uint32_t unpack_uint32()
  {
    uint32_t v;
    read(&v, 4);
    return v;
  }

  uint64_t unpack_uint64()
  {
    uint64_t v;
    read(&v, 8);
    return v;
  }

  template<size_t N>
  byte_array<N> unpack_bytes()
  {
    byte_array<N> a;
    read(a.data.data(), N);
    return a;
  }

  std::string unpack_string()
  {
    auto len = unpack_varint();
    check(len);
    std::string s(ptr, ptr + len);
    ptr += len;
    return s;
  }

  std::vector<char> unpack_vector_char()
  {
    auto len = unpack_varint();
    check(len);
    std::vector<char> v(ptr, ptr + len);
    ptr += len;
    return v;
  }

  key_data unpack_key_data()
  {
    auto priv = unpack_bytes<32>();  // private_key_type
    auto prefix = unpack_string();
    return {priv, std::move(prefix)};
  }

  keys_map unpack_keys_map()
  {
    auto count = unpack_varint();
    keys_map result;
    for (uint32_t i = 0; i < count; ++i)
    {
      auto pub = unpack_bytes<33>(); // public_key_type
      auto data = unpack_key_data();
      result.emplace(pub, std::move(data));
    }
    return result;
  }
};

// ── Convenience helpers ────────────────────────────────────────

/// Pack plain_keys = { sha512_hash checksum, keys_map keys }
/// Matches fc::raw::pack_to_vector(fc_plain_keys{checksum, keys})
inline std::vector<char> pack_plain_keys(const sha512_hash& checksum, const keys_map& keys)
{
  std::vector<char> buf;
  buf.reserve(64 + keys.size() * 70); // rough estimate
  packer p(buf);
  p.pack_bytes(checksum);
  p.pack_keys_map(keys);
  return buf;
}

/// Unpack plain_keys → (checksum, keys)
inline std::pair<sha512_hash, keys_map> unpack_plain_keys(const std::vector<char>& data)
{
  unpacker u(data);
  auto checksum = u.unpack_bytes<64>(); // sha512_hash
  auto keys = u.unpack_keys_map();
  return {checksum, std::move(keys)};
}

/// Pack crypto_data::content = { uint64_t nonce, uint32_t check, vector<char> encrypted }
/// Matches fc::raw::pack_to_vector(crypto_data::content{nonce, check, encrypted})
inline std::vector<char> pack_crypto_content(uint64_t nonce, uint32_t check,
                                              const std::vector<char>& encrypted)
{
  std::vector<char> buf;
  buf.reserve(12 + encrypted.size() + 4);
  packer p(buf);
  p.pack_uint64(nonce);
  p.pack_uint32(check);
  p.pack_vector_char(encrypted);
  return buf;
}

/// Unpack crypto_data::content → (nonce, check, encrypted)
struct crypto_content { uint64_t nonce; uint32_t check; std::vector<char> encrypted; };

inline crypto_content unpack_crypto_content(const std::vector<char>& data)
{
  unpacker u(data);
  auto nonce = u.unpack_uint64();
  auto check = u.unpack_uint32();
  auto encrypted = u.unpack_vector_char();
  return {nonce, check, std::move(encrypted)};
}

/// Pack a string for ECDH (content to encrypt): varint(len) + bytes
/// Matches fc::raw::pack_to_vector(std::string)
inline std::vector<char> pack_string(const std::string& s)
{
  std::vector<char> buf;
  packer p(buf);
  p.pack_string(s);
  return buf;
}

/// Unpack a string from packed bytes
inline std::string unpack_string(const std::vector<char>& data)
{
  unpacker u(data);
  return u.unpack_string();
}

// ── Hex encoding/decoding ──────────────────────────────────────

inline std::string hex_encode(const char* data, size_t len)
{
  static constexpr char hex_chars[] = "0123456789abcdef";
  std::string result;
  result.reserve(len * 2);
  for (size_t i = 0; i < len; ++i)
  {
    auto b = static_cast<uint8_t>(data[i]);
    result.push_back(hex_chars[b >> 4]);
    result.push_back(hex_chars[b & 0x0F]);
  }
  return result;
}

inline std::string hex_encode(const std::vector<char>& v)
{
  return hex_encode(v.data(), v.size());
}

inline uint8_t hex_digit(char c)
{
  if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
  if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
  throw std::runtime_error("binary_serializer: invalid hex character");
}

inline std::vector<char> hex_decode(const std::string& hex)
{
  if (hex.size() % 2 != 0)
    throw std::runtime_error("binary_serializer: odd-length hex string");

  std::vector<char> result;
  result.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2)
    result.push_back(static_cast<char>((hex_digit(hex[i]) << 4) | hex_digit(hex[i + 1])));
  return result;
}

} // namespace beekeeper_minimal
