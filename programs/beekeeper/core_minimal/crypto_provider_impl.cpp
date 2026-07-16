#include <core_minimal/crypto_provider_impl.hpp>
#include <core_minimal/binary_serializer.hpp>

#include <chrono>
#include <cstring>
#include <stdexcept>

namespace {

// ── v1 wallet blob layout ──────────────────────────────────────
//
// magic[7] "BEEKWLT" | version u8 | kdf_id u8 | iterations u32 LE |
// salt_len u8 | salt | iv_len u8 | iv | ciphertext | tag[32]
//
// ciphertext = AES-256-CBC(cipher_key, iv, pack(keys_map)), PKCS#7 padded.
// tag        = HMAC-SHA256(mac_key, all bytes before the tag).
// cipher_key = PBKDF2-HMAC-SHA512(password, salt, iterations, 64)[0:32]
// mac_key    =                                    ... same ... [32:64]
//
// Legacy blobs (written before this format) are raw AES-256-CBC ciphertext
// keyed by an unsalted SHA-512 of the password. The two formats are told
// apart by the magic prefix; a legacy blob whose first ciphertext bytes
// happen to equal the magic (probability ~2^-56 per wallet, as the first
// block is a pseudorandom function of the password) would be misparsed as
// v1 and become un-unlockable — an accepted residual risk.

constexpr uint8_t wallet_magic[7] = { 'B', 'E', 'E', 'K', 'W', 'L', 'T' };
constexpr uint8_t wallet_format_v1 = 1;
constexpr uint8_t kdf_pbkdf2_hmac_sha512 = 1;

constexpr size_t v1_salt_size     = 16;  // written by encrypt
constexpr size_t v1_min_salt_size = 8;   // accepted when parsing
constexpr size_t v1_max_salt_size = 64;  // accepted when parsing
constexpr size_t v1_iv_size       = 16;  // AES-CBC block size
constexpr size_t v1_tag_size      = 32;  // HMAC-SHA256
constexpr size_t v1_derived_key_size = 64;

bool has_wallet_magic(const std::vector<char>& blob)
{
  return blob.size() >= sizeof(wallet_magic) &&
         std::memcmp(blob.data(), wallet_magic, sizeof(wallet_magic)) == 0;
}

/// MAC verification must not leak a byte-position timing signal.
bool constant_time_equal(const uint8_t* a, const uint8_t* b, size_t len)
{
  uint8_t diff = 0;
  for (size_t i = 0; i < len; ++i)
    diff |= static_cast<uint8_t>(a[i] ^ b[i]);
  return diff == 0;
}

/// Best-effort wipe of key material (volatile keeps the compiler from eliding it).
void secure_wipe(void* buf, size_t len)
{
  auto p = static_cast<volatile uint8_t*>(buf);
  for (size_t i = 0; i < len; ++i)
    p[i] = 0;
}

} // anonymous namespace

namespace beekeeper_minimal {

crypto_provider_impl::crypto_provider_impl(crypto_primitives& prims, uint32_t kdf_iterations)
  : prims_(prims), kdf_iterations_(kdf_iterations)
{
  if (kdf_iterations_ == 0 || kdf_iterations_ > max_kdf_iterations)
    throw std::invalid_argument("KDF iteration count out of range");
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
  uint8_t decoded[64];
  size_t decoded_len = prims_.base58_decode(wif.data(), wif.size(), decoded, sizeof(decoded));

  if (decoded_len != 37)
    return std::nullopt;

  if (decoded[0] != 0x80)
    return std::nullopt;

  // Verify checksum: double SHA256 of first 33 bytes, take first 4
  auto hash1 = prims_.sha256(decoded, 33);
  auto hash2 = prims_.sha256(hash1.data.data(), 32);

  // FC accepts either single or double SHA256 for compatibility
  if (std::memcmp(hash2.data.data(), decoded + 33, 4) != 0 &&
      std::memcmp(hash1.data.data(), decoded + 33, 4) != 0)
    return std::nullopt;

  private_key_type key;
  std::memcpy(key.data.data(), decoded + 1, 32);
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

  char encoded[64]; // 37 * 138/100 + 2 = 53
  size_t encoded_len = prims_.base58_encode(data, 37, encoded, sizeof(encoded));
  return std::string(encoded, encoded_len);
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

  char encoded[64];
  size_t encoded_len = prims_.base58_encode(data, 37, encoded, sizeof(encoded));
  return prefix + std::string(encoded, encoded_len);
}

public_key_type crypto_provider_impl::public_key_from_string(const std::string& str,
                                                              const std::string& prefix)
{
  if (str.size() <= prefix.size() || str.substr(0, prefix.size()) != prefix)
    throw std::invalid_argument("public key requires prefix: " + prefix);

  auto b58part = str.substr(prefix.size());
  uint8_t decoded[64];
  size_t decoded_len = prims_.base58_decode(b58part.data(), b58part.size(), decoded, sizeof(decoded));
  if (decoded_len != 37)
    throw std::invalid_argument("invalid public key length");

  // Verify RIPEMD-160 checksum
  auto rmd = prims_.ripemd160(decoded, 33);
  if (std::memcmp(rmd.data(), decoded + 33, 4) != 0)
    throw std::invalid_argument("invalid public key checksum");

  public_key_type key;
  std::memcpy(key.data.data(), decoded, 33);
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

std::vector<char> crypto_provider_impl::encrypt_wallet_keys(
    const std::string& password, const keys_map& keys)
{
  return encrypt_wallet_keys_v1(password, keys);
}

std::vector<char> crypto_provider_impl::encrypt_wallet_keys_v1(
    const std::string& password, const keys_map& keys)
{
  // Note: an empty password is accepted. Current APIs generate a password
  // when none is given, but historical wallets created with an explicitly
  // empty password exist and must stay unlockable and re-encryptable.
  uint8_t salt[v1_salt_size];
  uint8_t iv[v1_iv_size];
  prims_.get_random_bytes(salt, sizeof(salt));
  prims_.get_random_bytes(iv, sizeof(iv));

  auto dk = prims_.pbkdf2_hmac_sha512(
    reinterpret_cast<const uint8_t*>(password.data()), password.size(),
    salt, sizeof(salt), kdf_iterations_, v1_derived_key_size);

  // aes_encrypt() takes key+IV packed as one 64-byte block: key[0:32], iv[32:48]
  sha512_hash key_iv;
  std::memcpy(key_iv.data.data(), dk.data(), 32);
  std::memcpy(key_iv.data.data() + 32, iv, v1_iv_size);

  std::vector<char> plain;
  plain.reserve(keys.size() * 80 + 16); // avoid reallocations that leave key copies behind
  packer keys_packer(plain);
  keys_packer.pack_keys_map(keys);

  auto ciphertext = aes_encrypt(key_iv, plain);
  secure_wipe(plain.data(), plain.size());

  std::vector<char> blob;
  blob.reserve(64 + v1_salt_size + v1_iv_size + ciphertext.size() + v1_tag_size);
  packer p(blob);
  p.write(wallet_magic, sizeof(wallet_magic));
  p.pack_uint8(wallet_format_v1);
  p.pack_uint8(kdf_pbkdf2_hmac_sha512);
  p.pack_uint32(kdf_iterations_);
  p.pack_uint8(v1_salt_size);
  p.write(salt, sizeof(salt));
  p.pack_uint8(v1_iv_size);
  p.write(iv, sizeof(iv));
  p.write(ciphertext.data(), ciphertext.size());

  auto tag = prims_.hmac_sha256(
    dk.data() + 32, 32,
    reinterpret_cast<const uint8_t*>(blob.data()), blob.size());
  p.write(tag.data.data(), v1_tag_size);

  secure_wipe(dk.data(), dk.size());
  secure_wipe(key_iv.data.data(), key_iv.size());
  return blob;
}

std::vector<char> crypto_provider_impl::encrypt_wallet_data(
    const std::string& password, const keys_map& keys)
{
  auto cipher_keys = encrypt_wallet_keys(password, keys);

  // FC's wallet_data serializes as just the cipher_keys vector (hex-encoded string in JSON)
  auto hex = hex_encode(cipher_keys);
  std::string json = "\"" + hex + "\"";

  return std::vector<char>(json.begin(), json.end());
}

keys_map crypto_provider_impl::decrypt_wallet_data(
    const std::string& password, const std::vector<char>& cipher_keys)
{
  if (has_wallet_magic(cipher_keys))
    return decrypt_wallet_data_v1(password, cipher_keys);
  return decrypt_wallet_data_legacy(password, cipher_keys);
}

keys_map crypto_provider_impl::decrypt_wallet_data_v1(
    const std::string& password, const std::vector<char>& cipher_keys)
{
  unpacker u(cipher_keys);

  uint8_t magic[sizeof(wallet_magic)];
  u.read(magic, sizeof(magic)); // presence already checked by the dispatcher

  auto version = u.unpack_uint8();
  if (version != wallet_format_v1)
    throw std::runtime_error("Unsupported wallet format version");

  auto kdf_id = u.unpack_uint8();
  if (kdf_id != kdf_pbkdf2_hmac_sha512)
    throw std::runtime_error("Unsupported wallet KDF");

  auto iterations = u.unpack_uint32();
  if (iterations == 0 || iterations > max_kdf_iterations)
    throw std::runtime_error("Wallet KDF iteration count out of range");

  auto salt_len = u.unpack_uint8();
  if (salt_len < v1_min_salt_size || salt_len > v1_max_salt_size)
    throw std::runtime_error("Wallet KDF salt length out of range");
  uint8_t salt[v1_max_salt_size];
  u.read(salt, salt_len);

  auto iv_len = u.unpack_uint8();
  if (iv_len != v1_iv_size)
    throw std::runtime_error("Wallet cipher IV length invalid");
  uint8_t iv[v1_iv_size];
  u.read(iv, iv_len);

  // Everything left is ciphertext followed by the 32-byte tag.
  // CBC output of a non-empty plaintext is at least one block.
  if (u.remaining() < v1_tag_size + 16)
    throw std::runtime_error("Wallet data truncated");
  size_t cipher_len = u.remaining() - v1_tag_size;
  if (cipher_len % 16 != 0)
    throw std::runtime_error("Wallet ciphertext length invalid");

  auto dk = prims_.pbkdf2_hmac_sha512(
    reinterpret_cast<const uint8_t*>(password.data()), password.size(),
    salt, salt_len, iterations, v1_derived_key_size);

  size_t mac_covered = cipher_keys.size() - v1_tag_size;
  auto expected_tag = prims_.hmac_sha256(
    dk.data() + 32, 32,
    reinterpret_cast<const uint8_t*>(cipher_keys.data()), mac_covered);

  auto stored_tag = reinterpret_cast<const uint8_t*>(cipher_keys.data()) + mac_covered;
  if (!constant_time_equal(expected_tag.data.data(), stored_tag, v1_tag_size))
  {
    secure_wipe(dk.data(), dk.size());
    // A wrong password and a tampered blob are indistinguishable here by design.
    throw std::runtime_error("Invalid password");
  }

  sha512_hash key_iv;
  std::memcpy(key_iv.data.data(), dk.data(), 32);
  std::memcpy(key_iv.data.data() + 32, iv, v1_iv_size);
  secure_wipe(dk.data(), dk.size());

  std::vector<char> ciphertext(u.ptr, u.ptr + cipher_len);
  auto decrypted = aes_decrypt(key_iv, ciphertext);
  secure_wipe(key_iv.data.data(), key_iv.size());

  unpacker keys_unpacker(decrypted);
  auto keys = keys_unpacker.unpack_keys_map();
  secure_wipe(decrypted.data(), decrypted.size());
  return keys;
}

keys_map crypto_provider_impl::decrypt_wallet_data_legacy(
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

bool crypto_provider_impl::is_legacy_wallet(const std::vector<char>& cipher_keys) const
{
  return !has_wallet_magic(cipher_keys);
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
  size_t enc_buf_size = packed.size() * 138 / 100 + 2;
  std::vector<char> enc_buf(enc_buf_size);
  size_t enc_len = prims_.base58_encode(
    reinterpret_cast<const uint8_t*>(packed.data()),
    packed.size(), enc_buf.data(), enc_buf_size);

  return std::string(enc_buf.data(), enc_len);
}

std::string crypto_provider_impl::ecdh_decrypt(
    const private_key_type& priv_key, const public_key_type& other_pub,
    const std::string& encrypted_content)
{
  // 1. Base58-decode and unpack content
  size_t dec_buf_size = encrypted_content.size() * 733 / 1000 + 2;
  std::vector<uint8_t> dec_buf(dec_buf_size);
  size_t dec_len = prims_.base58_decode(
    encrypted_content.data(), encrypted_content.size(),
    dec_buf.data(), dec_buf_size);
  auto packed = std::vector<char>(
    reinterpret_cast<const char*>(dec_buf.data()),
    reinterpret_cast<const char*>(dec_buf.data()) + dec_len);
  auto content = unpack_crypto_content(packed);

  // 2. Compute shared secret
  auto shared = prims_.ecdh_shared_secret(priv_key, other_pub);

  // 3. Regenerate encryption key
  auto enc_key = generate_encrypted_key(content.nonce, shared);

  // 4. Verify check
  auto check_hash = prims_.sha256(enc_key.data.data(), 64);
  uint32_t expected_check;
  std::memcpy(&expected_check, check_hash.data.data(), 4);
  if (expected_check != content.check)
    throw std::runtime_error("ECDH decryption check mismatch");

  // 5. AES-decrypt and unpack string
  auto decrypted = aes_decrypt(enc_key, content.encrypted);
  return unpack_string(decrypted);
}

void crypto_provider_impl::get_random_bytes(uint8_t* buf, size_t len)
{
  prims_.get_random_bytes(buf, len);
}

} // namespace beekeeper_minimal
