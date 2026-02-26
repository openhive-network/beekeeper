#include <core_minimal/crypto_provider_impl.hpp>
#include <core_minimal/binary_serializer.hpp>

#include <chrono>
#include <cstring>
#include <stdexcept>

namespace beekeeper_minimal {

crypto_provider_impl::crypto_provider_impl(crypto_primitives& prims)
  : prims_(prims)
{
}

// ── helpers ────────────────────────────────────────────────────

std::vector<char> crypto_provider_impl::aes_encrypt(const sha512_hash& hash,
                                                     const std::vector<char>& data)
{
  // Key = first 32 bytes, IV = bytes 32..47 (16 bytes)
  auto encrypted = prims_.aes256_cbc_encrypt(
    hash.data.data(),        // key: 32 bytes
    hash.data.data() + 32,   // iv:  16 bytes
    reinterpret_cast<const uint8_t*>(data.data()),
    data.size());

  return std::vector<char>(
    reinterpret_cast<const char*>(encrypted.data()),
    reinterpret_cast<const char*>(encrypted.data()) + encrypted.size());
}

std::vector<char> crypto_provider_impl::aes_decrypt(const sha512_hash& hash,
                                                     const std::vector<char>& data)
{
  auto decrypted = prims_.aes256_cbc_decrypt(
    hash.data.data(),
    hash.data.data() + 32,
    reinterpret_cast<const uint8_t*>(data.data()),
    data.size());

  return std::vector<char>(
    reinterpret_cast<const char*>(decrypted.data()),
    reinterpret_cast<const char*>(decrypted.data()) + decrypted.size());
}

sha512_hash crypto_provider_impl::generate_encrypted_key(uint64_t nonce,
                                                          const sha512_hash& shared_secret)
{
  // Matches FC: sha512(pack(nonce) || pack(shared_secret))
  // pack(uint64_t) = 8 bytes little-endian
  // pack(sha512)   = 64 raw bytes
  uint8_t buf[8 + 64];
  std::memcpy(buf, &nonce, 8);          // little-endian uint64
  std::memcpy(buf + 8, shared_secret.data.data(), 64);
  return prims_.sha512(buf, sizeof(buf));
}

// ── Key operations ─────────────────────────────────────────────

private_key_type crypto_provider_impl::generate_private_key()
{
  return prims_.generate_private_key();
}

std::optional<private_key_type> crypto_provider_impl::wif_to_key(const std::string& wif)
{
  // WIF format: base58(0x80 + 32-byte-key + 4-byte-checksum)
  // Total raw = 37 bytes
  std::vector<uint8_t> decoded;
  try
  {
    decoded = prims_.base58_decode(wif);
  }
  catch (...)
  {
    return std::nullopt;
  }

  if (decoded.size() != 37)
    return std::nullopt;

  if (decoded[0] != 0x80)
    return std::nullopt;

  // Verify checksum: double SHA256 of first 33 bytes, take first 4
  auto hash1 = prims_.sha256(decoded.data(), 33);
  auto hash2 = prims_.sha256(hash1.data.data(), 32);

  // FC accepts either single or double SHA256 for compatibility
  if (std::memcmp(hash2.data.data(), decoded.data() + 33, 4) != 0 &&
      std::memcmp(hash1.data.data(), decoded.data() + 33, 4) != 0)
    return std::nullopt;

  private_key_type key;
  std::memcpy(key.data.data(), decoded.data() + 1, 32);
  return key;
}

std::string crypto_provider_impl::key_to_wif(const private_key_type& key)
{
  // 0x80 + 32-byte key + 4-byte double-SHA256 checksum
  uint8_t data[37];
  data[0] = 0x80;
  std::memcpy(data + 1, key.data.data(), 32);

  auto hash1 = prims_.sha256(data, 33);
  auto hash2 = prims_.sha256(hash1.data.data(), 32);
  std::memcpy(data + 33, hash2.data.data(), 4);

  return prims_.base58_encode(data, 37);
}

public_key_type crypto_provider_impl::get_public_key(const private_key_type& key)
{
  return prims_.get_public_key(key);
}

std::string crypto_provider_impl::public_key_to_string(const public_key_type& key,
                                                        const std::string& prefix)
{
  // Format: prefix + base58(33-byte-compressed-key + 4-byte-ripemd160-checksum)
  auto rmd = prims_.ripemd160(key.data.data(), 33);

  uint8_t data[37]; // 33 + 4
  std::memcpy(data, key.data.data(), 33);
  std::memcpy(data + 33, rmd.data(), 4);

  return prefix + prims_.base58_encode(data, 37);
}

public_key_type crypto_provider_impl::public_key_from_string(const std::string& str,
                                                              const std::string& prefix)
{
  if (str.size() <= prefix.size() || str.substr(0, prefix.size()) != prefix)
    throw std::invalid_argument("public key requires prefix: " + prefix);

  auto decoded = prims_.base58_decode(str.substr(prefix.size()));
  if (decoded.size() != 37)
    throw std::invalid_argument("invalid public key length");

  // Verify RIPEMD-160 checksum
  auto rmd = prims_.ripemd160(decoded.data(), 33);
  if (std::memcmp(rmd.data(), decoded.data() + 33, 4) != 0)
    throw std::invalid_argument("invalid public key checksum");

  public_key_type key;
  std::memcpy(key.data.data(), decoded.data(), 33);
  return key;
}

// ── Signing ────────────────────────────────────────────────────

signature_type crypto_provider_impl::sign_compact(const private_key_type& key,
                                                   const digest_type& digest)
{
  return prims_.sign_compact(key, digest);
}

// ── Digest / signature hex ─────────────────────────────────────

digest_type crypto_provider_impl::digest_from_hex(const std::string& hex_str)
{
  auto bytes = hex_decode(hex_str);
  if (bytes.size() != 32)
    throw std::runtime_error("expected 64 hex chars for digest");

  digest_type d;
  std::memcpy(d.data.data(), bytes.data(), 32);
  return d;
}

std::string crypto_provider_impl::signature_to_hex(const signature_type& sig)
{
  return hex_encode(reinterpret_cast<const char*>(sig.data.data()), sig.size());
}

// ── Wallet encryption ──────────────────────────────────────────

std::vector<char> crypto_provider_impl::encrypt_wallet_data(
    const std::string& password, const keys_map& keys)
{
  // 1. Hash password → sha512
  auto pw = prims_.sha512(
    reinterpret_cast<const uint8_t*>(password.data()),
    password.size());

  // 2. Pack plain_keys = { checksum=pw, keys=keys }
  auto plain_txt = pack_plain_keys(pw, keys);

  // 3. AES-encrypt with pw as key+IV
  auto cipher_keys = aes_encrypt(pw, plain_txt);

  // 4. Serialize as wallet file JSON
  // FC's wallet_data serializes as just the cipher_keys vector (hex-encoded string in JSON)
  auto hex = hex_encode(cipher_keys);
  std::string json = "\"" + hex + "\"";

  return std::vector<char>(json.begin(), json.end());
}

keys_map crypto_provider_impl::decrypt_wallet_data(
    const std::string& password, const std::vector<char>& cipher_keys)
{
  auto pw = prims_.sha512(
    reinterpret_cast<const uint8_t*>(password.data()),
    password.size());

  std::vector<char> decrypted;
  try
  {
    decrypted = aes_decrypt(pw, cipher_keys);
  }
  catch (...)
  {
    throw std::runtime_error("Invalid password");
  }

  // Validate checksum: first 64 bytes of unpacked data should be sha512(password)
  auto [stored_checksum, keys] = unpack_plain_keys(decrypted);
  if (pw != stored_checksum)
    throw std::runtime_error("Invalid password");

  return keys;
}

std::vector<char> crypto_provider_impl::parse_wallet_file(
    const std::vector<char>& wallet_file_content)
{
  // Wallet file is a JSON hex string: "a1b2c3..."
  // Strip surrounding quotes and whitespace, then hex-decode
  std::string json(wallet_file_content.begin(), wallet_file_content.end());

  // Find the hex content between quotes
  auto start = json.find('"');
  auto end = json.rfind('"');
  if (start == std::string::npos || end == std::string::npos || start == end)
    throw std::runtime_error("Invalid wallet file format");

  std::string hex = json.substr(start + 1, end - start - 1);
  return hex_decode(hex);
}

void crypto_provider_impl::validate_password(
    const std::string& password, const std::vector<char>& cipher_keys)
{
  decrypt_wallet_data(password, cipher_keys);
}

// ── ECDH encrypt / decrypt ─────────────────────────────────────

std::string crypto_provider_impl::ecdh_encrypt(
    const private_key_type& from_key, const public_key_type& to_key,
    const std::string& content, std::optional<uint64_t> nonce_opt)
{
  // 1. Compute ECDH shared secret (SHA512 of raw ECDH point)
  auto shared = prims_.ecdh_shared_secret(from_key, to_key);

  // 2. Determine nonce (default: current time in microseconds)
  uint64_t nonce = nonce_opt.value_or(
    static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count()));

  // 3. Generate encryption key: SHA512(pack(nonce) || shared_secret)
  auto enc_key = generate_encrypted_key(nonce, shared);

  // 4. Compute check: first 4 bytes of SHA256(enc_key)
  auto check_hash = prims_.sha256(enc_key.data.data(), 64);
  uint32_t check;
  std::memcpy(&check, check_hash.data.data(), 4);

  // 5. AES-encrypt the packed content
  auto packed_content = pack_string(content);
  auto encrypted = aes_encrypt(enc_key, packed_content);

  // 6. Pack crypto_data::content and Base58-encode
  auto packed = pack_crypto_content(nonce, check, encrypted);
  auto encoded = prims_.base58_encode(
    reinterpret_cast<const uint8_t*>(packed.data()),
    packed.size());

  return encoded;
}

std::string crypto_provider_impl::ecdh_decrypt(
    key_finder_type key_finder, const public_key_type& from_key,
    const public_key_type& to_key, const std::string& encrypted_content)
{
  // 1. Base58-decode and unpack content
  auto decoded = prims_.base58_decode(encrypted_content);
  auto packed = std::vector<char>(
    reinterpret_cast<const char*>(decoded.data()),
    reinterpret_cast<const char*>(decoded.data()) + decoded.size());
  auto content = unpack_crypto_content(packed);

  // 2. Find private key — try both from_key and to_key
  auto priv = key_finder(to_key);
  const public_key_type* other_pub = &from_key;
  if (!priv)
  {
    priv = key_finder(from_key);
    other_pub = &to_key;
  }
  if (!priv)
    throw std::runtime_error("No matching private key found for ECDH decryption");

  // 3. Compute shared secret
  auto shared = prims_.ecdh_shared_secret(*priv, *other_pub);

  // 4. Regenerate encryption key
  auto enc_key = generate_encrypted_key(content.nonce, shared);

  // 5. Verify check
  auto check_hash = prims_.sha256(enc_key.data.data(), 64);
  uint32_t expected_check;
  std::memcpy(&expected_check, check_hash.data.data(), 4);
  if (expected_check != content.check)
    throw std::runtime_error("ECDH decryption check mismatch");

  // 6. AES-decrypt and unpack string
  auto decrypted = aes_decrypt(enc_key, content.encrypted);
  return unpack_string(decrypted);
}

void crypto_provider_impl::get_random_bytes(uint8_t* buf, size_t len)
{
  prims_.get_random_bytes(buf, len);
}

} // namespace beekeeper_minimal
