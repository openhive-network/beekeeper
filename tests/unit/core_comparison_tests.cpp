/**
 * Comparison tests: core (original) vs core_minimal
 *
 * These tests verify that both implementations produce identical results
 * for wallet encryption, key management, and signing operations.
 * They share the same serialized wallet data to prove format compatibility.
 */

#define BOOST_TEST_MODULE core_comparison_tests

#include <boost/test/included/unit_test.hpp>

// ── original core ────────────────────────────────────────────
#include <core/wallet_content_handler.hpp>
#include <core/utilities.hpp>

// ── minimal core ─────────────────────────────────────────────
#include <core_minimal/beekeeper.hpp>

// ── FC crypto bridge ─────────────────────────────────────────
#include <fc_crypto_bridge/fc_crypto_provider.hpp>

#include <fc/io/json.hpp>
#include <fc/io/raw.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/filesystem.hpp>

#include <fstream>
#include <filesystem>

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────

namespace {

/// Shared crypto provider for all tests
beekeeper_minimal::fc_crypto_provider crypto;

/// Convert an FC public key to a core_minimal public_key_type via string round-trip
beekeeper_minimal::public_key_type to_min_pubkey(const fc::ecc::public_key& fc_pub,
                                                  const std::string& prefix = "STM")
{
  auto str = beekeeper::utility::public_key::to_string(fc_pub, prefix);
  return crypto.public_key_from_string(str, prefix);
}

/// Convert an FC sha256 to a core_minimal digest_type
beekeeper_minimal::digest_type to_min_digest(const fc::sha256& fc_dig)
{
  auto hex = fc_dig.str();
  return crypto.digest_from_hex(hex);
}

/// Convert an FC compact_signature to a hex string for comparison
std::string fc_sig_to_hex(const fc::ecc::compact_signature& sig)
{
  return fc::to_hex(reinterpret_cast<const char*>(&sig), sizeof(sig));
}

/// Convert a core_minimal signature_type to a hex string for comparison
std::string min_sig_to_hex(const beekeeper_minimal::signature_type& sig)
{
  return crypto.signature_to_hex(sig);
}

/// An in-memory wallet_storage for core_minimal tests.
/// Stores blobs in a map keyed by path.
struct memory_storage : beekeeper_minimal::wallet_storage
{
  std::map<std::string, std::vector<char>> blobs;

  void save(const std::string& path, const std::vector<char>& buffer) override
  {
    blobs[path] = buffer;
  }

  std::vector<char> load(const std::string& path) override
  {
    auto it = blobs.find(path);
    if (it == blobs.end())
      throw std::runtime_error("Not found: " + path);
    return it->second;
  }

  std::vector<std::string> list_dir() override
  {
    std::vector<std::string> names;
    for (auto& [k, v] : blobs)
      names.push_back(k);
    return names;
  }
};

/// Deterministic test keys (WIF format)
constexpr auto wif_key1 = "5JktVNHnRX48BUdtewU7N1CyL4Z886c42x7wYW7XhNWkDQRhdcS";
constexpr auto wif_key2 = "5Ju5RTcVDo35ndtzHioPMgebvBM6LkJ6tvuU6LTNQv8yaz3ggZr";
constexpr auto wif_key3 = "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3";

constexpr auto prefix   = "STM";
constexpr auto password = "my-test-password";

/// Temporary directory that cleans up after itself.
struct tmp_dir
{
  fc::path dir;
  tmp_dir() : dir(fc::current_path() / "comparison-test-storage")
  {
    fc::create_directories(dir);
  }
  ~tmp_dir()
  {
    try { fc::remove_all(dir); } catch (...) {}
  }
};

/// File-based wallet_storage for cross-load tests
struct file_storage : beekeeper_minimal::wallet_storage
{
  void save(const std::string& path, const std::vector<char>& buf) override
  {
    std::ofstream f(path, std::ios::binary);
    f.write(buf.data(), buf.size());
  }
  std::vector<char> load(const std::string& path) override
  {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    auto sz = f.tellg();
    f.seekg(0);
    std::vector<char> buf(sz);
    f.read(buf.data(), sz);
    return buf;
  }

  std::vector<std::string> list_dir() override
  {
    return {}; // Not needed for cross-load tests
  }
};

} // anon

// ─────────────────────────────────────────────────────────────
BOOST_AUTO_TEST_SUITE(core_comparison)
// ─────────────────────────────────────────────────────────────

/// 1. Create a wallet with the original core, save it.
///    Load it with core_minimal and verify same keys come out.
BOOST_AUTO_TEST_CASE(cross_load_original_to_minimal)
{
  tmp_dir td;
  auto wallet_file = (td.dir / "cross1").string();

  // ── original: create, import, save ──
  beekeeper::wallet_content_handler original;
  original.set_password(password);
  original.set_wallet_name(wallet_file);
  original.unlock(password);
  std::string pub1_orig = original.import_key(wif_key1, prefix);
  std::string pub2_orig = original.import_key(wif_key2, prefix);
  original.save_wallet_file();
  auto orig_keys = original.get_keys_details();

  // ── minimal: load same file via storage ──
  file_storage fs;

  beekeeper_minimal::wallet minimal(crypto, &fs, wallet_file);
  minimal.open();
  BOOST_REQUIRE(minimal.is_locked());

  minimal.unlock(password);
  BOOST_REQUIRE(!minimal.is_locked());

  auto& min_keys = minimal.get_keys();
  BOOST_REQUIRE_EQUAL(min_keys.size(), orig_keys.size());

  // Verify every key pair matches (compare via WIF string)
  for (auto& [fc_pub, fc_data] : orig_keys)
  {
    auto min_pub = to_min_pubkey(fc_pub);
    auto it = min_keys.find(min_pub);
    BOOST_REQUIRE(it != min_keys.end());
    BOOST_REQUIRE_EQUAL(crypto.key_to_wif(it->second.first), fc_data.first.key_to_wif());
    BOOST_REQUIRE_EQUAL(it->second.second, fc_data.second);
  }
}

/// 2. Create with core_minimal, load with original core.
BOOST_AUTO_TEST_CASE(cross_load_minimal_to_original)
{
  tmp_dir td;
  auto wallet_file = (td.dir / "cross2").string();

  // ── minimal: create, import, persist ──
  file_storage fs;

  beekeeper_minimal::wallet minimal(crypto, &fs, wallet_file);
  minimal.create(password);
  std::string pub1_min = minimal.import_key(wif_key1, prefix);
  std::string pub2_min = minimal.import_key(wif_key2, prefix);
  auto min_keys = minimal.get_keys();

  // ── original: load same file ──
  beekeeper::wallet_content_handler original;
  original.set_wallet_name(wallet_file);
  BOOST_REQUIRE(original.load_wallet_file());
  BOOST_REQUIRE(original.is_locked());

  original.unlock(password);
  BOOST_REQUIRE(!original.is_locked());

  auto orig_keys = original.get_keys_details();
  BOOST_REQUIRE_EQUAL(orig_keys.size(), min_keys.size());

  for (auto& [min_pub, min_data] : min_keys)
  {
    auto pub_str = crypto.public_key_to_string(min_pub, prefix);
    auto fc_pub = beekeeper::utility::public_key::create(pub_str, prefix);
    auto it = orig_keys.find(fc_pub);
    BOOST_REQUIRE(it != orig_keys.end());
    BOOST_REQUIRE_EQUAL(it->second.first.key_to_wif(), crypto.key_to_wif(min_data.first));
    BOOST_REQUIRE_EQUAL(it->second.second, min_data.second);
  }
}

/// 3. Same password + same keys ⇒ encryption produces same decryptable content
///    (AES is deterministic given the same checksum)
BOOST_AUTO_TEST_CASE(encryption_decryption_equivalence)
{
  memory_storage ms;

  // ── minimal ──
  beekeeper_minimal::wallet w_min(crypto, &ms, "enc_test");
  w_min.create(password);
  w_min.import_key(wif_key1, prefix);
  w_min.import_key(wif_key2, prefix);
  w_min.import_key(wif_key3, prefix);

  // Get the encrypted blob produced by minimal
  BOOST_REQUIRE(ms.blobs.count("enc_test"));
  auto min_blob = ms.blobs["enc_test"];
  std::string min_json(min_blob.begin(), min_blob.end());

  // Parse it the same way the original would
  auto min_wd = fc::json::from_string(min_json, fc::json::format_validation_mode::full).as<beekeeper::wallet_data>();

  // Decrypt using original's algorithm
  auto pw_hash = fc::sha512::hash(password, strlen(password));
  auto decrypted = fc::aes_decrypt(pw_hash, min_wd.cipher_keys);

  beekeeper::plain_keys pk;
  fc::raw::unpack_from_vector<beekeeper::plain_keys>(decrypted, pk, 0, true);

  BOOST_REQUIRE_EQUAL(pk.keys.size(), 3u);
  BOOST_REQUIRE(pk.checksum == pw_hash);

  // Each key should match
  auto k1 = beekeeper::private_key_type::wif_to_key(wif_key1).value();
  auto k2 = beekeeper::private_key_type::wif_to_key(wif_key2).value();
  auto k3 = beekeeper::private_key_type::wif_to_key(wif_key3).value();

  BOOST_REQUIRE(pk.keys.find(k1.get_public_key()) != pk.keys.end());
  BOOST_REQUIRE(pk.keys.find(k2.get_public_key()) != pk.keys.end());
  BOOST_REQUIRE(pk.keys.find(k3.get_public_key()) != pk.keys.end());
}

/// 4. Both produce the same signature for the same digest + key
BOOST_AUTO_TEST_CASE(sign_digest_same_result)
{
  memory_storage ms;

  auto priv1 = fc::ecc::private_key::wif_to_key(wif_key1).value();
  auto pub1  = priv1.get_public_key();

  // A deterministic digest
  auto digest = fc::sha256::hash("transaction data to sign");

  // ── original (temporary = no file I/O) ──
  beekeeper::wallet_content_handler original(true);
  original.set_password(password);
  original.unlock(password);
  original.import_key(wif_key1, prefix);

  auto sig_orig = original.try_sign_digest(digest, pub1);
  BOOST_REQUIRE(sig_orig.has_value());

  // ── minimal ──
  beekeeper_minimal::wallet w_min(crypto, &ms, "sign_test");
  w_min.create(password);
  w_min.import_key(wif_key1, prefix);

  auto min_pub1 = to_min_pubkey(pub1);
  auto min_digest = to_min_digest(digest);
  auto sig_min = w_min.try_sign_digest(min_digest, min_pub1);
  BOOST_REQUIRE(sig_min.has_value());

  // Compare via hex string
  BOOST_REQUIRE_EQUAL(fc_sig_to_hex(*sig_orig), min_sig_to_hex(*sig_min));
}

/// 5. Both produce the same public key string from an imported WIF key
BOOST_AUTO_TEST_CASE(import_key_same_public_key)
{
  memory_storage ms;

  // ── original (temporary = no file I/O) ──
  beekeeper::wallet_content_handler original(true);
  original.set_password(password);
  original.unlock(password);
  std::string pub_orig = original.import_key(wif_key1, prefix);

  // ── minimal ──
  beekeeper_minimal::wallet w_min(crypto, &ms, "import_test");
  w_min.create(password);
  std::string pub_min = w_min.import_key(wif_key1, prefix);

  BOOST_REQUIRE_EQUAL(pub_orig, pub_min);
}

/// 6. Lock/unlock cycle preserves keys identically in both
BOOST_AUTO_TEST_CASE(lock_unlock_preserves_keys)
{
  memory_storage ms;

  // ── original (in-memory/temporary) ──
  beekeeper::wallet_content_handler original(true /*is_temporary*/);
  original.set_password(password);
  original.unlock(password);
  original.import_key(wif_key1, prefix);
  original.import_key(wif_key2, prefix);

  original.lock();
  BOOST_REQUIRE(original.is_locked());
  original.unlock(password);
  BOOST_REQUIRE(!original.is_locked());
  auto orig_keys = original.get_keys_details();

  // ── minimal (in-memory) ──
  beekeeper_minimal::wallet w_min(crypto, &ms, "lock_test");
  w_min.create(password);
  w_min.import_key(wif_key1, prefix);
  w_min.import_key(wif_key2, prefix);

  w_min.lock();
  BOOST_REQUIRE(w_min.is_locked());
  w_min.unlock(password);
  BOOST_REQUIRE(!w_min.is_locked());
  auto& min_keys = w_min.get_keys();

  BOOST_REQUIRE_EQUAL(orig_keys.size(), min_keys.size());
  for (auto& [fc_pub, fc_data] : orig_keys)
  {
    auto min_pub = to_min_pubkey(fc_pub);
    auto it = min_keys.find(min_pub);
    BOOST_REQUIRE(it != min_keys.end());
    BOOST_REQUIRE_EQUAL(crypto.key_to_wif(it->second.first), fc_data.first.key_to_wif());
  }
}

/// 7. Removing a key gives the same remaining set
BOOST_AUTO_TEST_CASE(remove_key_same_result)
{
  memory_storage ms;

  auto priv2 = fc::ecc::private_key::wif_to_key(wif_key2).value();
  auto pub2  = priv2.get_public_key();
  auto min_pub2 = to_min_pubkey(pub2);

  // ── original ──
  beekeeper::wallet_content_handler original(true);
  original.set_password(password);
  original.unlock(password);
  original.import_key(wif_key1, prefix);
  original.import_key(wif_key2, prefix);
  original.import_key(wif_key3, prefix);
  original.remove_key(pub2);
  auto orig_keys = original.get_keys_details();

  // ── minimal ──
  beekeeper_minimal::wallet w_min(crypto, &ms, "remove_test");
  w_min.create(password);
  w_min.import_key(wif_key1, prefix);
  w_min.import_key(wif_key2, prefix);
  w_min.import_key(wif_key3, prefix);
  w_min.remove_key(min_pub2);
  auto& min_keys = w_min.get_keys();

  BOOST_REQUIRE_EQUAL(orig_keys.size(), 2u);
  BOOST_REQUIRE_EQUAL(min_keys.size(), 2u);

  // key2 should be gone in both
  BOOST_REQUIRE(orig_keys.find(pub2) == orig_keys.end());
  BOOST_REQUIRE(min_keys.find(min_pub2) == min_keys.end());

  for (auto& [fc_pub, fc_data] : orig_keys)
  {
    auto min_pub = to_min_pubkey(fc_pub);
    auto it = min_keys.find(min_pub);
    BOOST_REQUIRE(it != min_keys.end());
    BOOST_REQUIRE_EQUAL(crypto.key_to_wif(it->second.first), fc_data.first.key_to_wif());
  }
}

/// 8. Bad password throws in both (and doesn't corrupt state)
BOOST_AUTO_TEST_CASE(wrong_password_throws_in_both)
{
  memory_storage ms;

  // ── original ──
  beekeeper::wallet_content_handler original(true);
  original.set_password(password);
  original.unlock(password);
  original.import_key(wif_key1, prefix);
  original.lock();

  BOOST_REQUIRE_THROW(original.unlock("wrong-password"), fc::exception);
  BOOST_REQUIRE(original.is_locked());

  // ── minimal ──
  beekeeper_minimal::wallet w_min(crypto, &ms, "badpw_test");
  w_min.create(password);
  w_min.import_key(wif_key1, prefix);
  w_min.lock();

  BOOST_REQUIRE_THROW(w_min.unlock("wrong-password"), std::runtime_error);
  BOOST_REQUIRE(w_min.is_locked());
}

/// 9. has_matching_private_key agrees for both
BOOST_AUTO_TEST_CASE(has_matching_private_key_agrees)
{
  memory_storage ms;

  auto priv1 = fc::ecc::private_key::wif_to_key(wif_key1).value();
  auto pub1  = priv1.get_public_key();
  auto priv3 = fc::ecc::private_key::wif_to_key(wif_key3).value();
  auto pub3  = priv3.get_public_key();

  auto min_pub1 = to_min_pubkey(pub1);
  auto min_pub3 = to_min_pubkey(pub3);

  // ── original ──
  beekeeper::wallet_content_handler original(true);
  original.set_password(password);
  original.unlock(password);
  original.import_key(wif_key1, prefix);

  BOOST_REQUIRE(original.has_matching_private_key(pub1));
  BOOST_REQUIRE(!original.has_matching_private_key(pub3));

  // ── minimal ──
  beekeeper_minimal::wallet w_min(crypto, &ms, "has_key_test");
  w_min.create(password);
  w_min.import_key(wif_key1, prefix);

  BOOST_REQUIRE(w_min.has_private_key(min_pub1));
  BOOST_REQUIRE(!w_min.has_private_key(min_pub3));
}

/// 10. Session-level: sign_digest across multiple wallets gives same result
BOOST_AUTO_TEST_CASE(session_sign_across_wallets)
{
  memory_storage ms;

  auto priv1 = fc::ecc::private_key::wif_to_key(wif_key1).value();
  auto pub1  = priv1.get_public_key();
  auto priv3 = fc::ecc::private_key::wif_to_key(wif_key3).value();
  auto pub3  = priv3.get_public_key();
  auto digest = fc::sha256::hash("session sign test data");

  auto min_pub3 = to_min_pubkey(pub3);
  auto min_digest = to_min_digest(digest);

  // ── minimal beekeeper ──
  beekeeper_minimal::beekeeper bk(crypto, ms, 900);
  auto token = bk.create_session("test-salt");

  bk.create_wallet(token, "w1", password);
  bk.import_key("w1", wif_key1, prefix);
  bk.create_wallet(token, "w2", "password2");
  bk.import_key("w2", wif_key3, prefix);

  // Sign without specifying wallet (searches all in session)
  auto sig_min = bk.sign_digest(token, "", min_digest, min_pub3, prefix);

  // ── original (direct wallet) ──
  beekeeper::wallet_content_handler original(true);
  original.set_password("password2");
  original.unlock("password2");
  original.import_key(wif_key3, prefix);
  auto sig_orig = original.try_sign_digest(digest, pub3);
  BOOST_REQUIRE(sig_orig.has_value());

  BOOST_REQUIRE_EQUAL(min_sig_to_hex(sig_min), fc_sig_to_hex(*sig_orig));
}

/// 11. Verify check_password works identically
BOOST_AUTO_TEST_CASE(check_password_equivalence)
{
  memory_storage ms;

  // ── original ──
  beekeeper::wallet_content_handler original(true);
  original.set_password(password);
  original.unlock(password);
  original.import_key(wif_key1, prefix);

  BOOST_REQUIRE_NO_THROW(original.check_password(password));
  BOOST_REQUIRE_THROW(original.check_password("wrong"), fc::exception);

  // ── minimal ──
  beekeeper_minimal::wallet w_min(crypto, &ms, "checkpw_test");
  w_min.create(password);
  w_min.import_key(wif_key1, prefix);

  BOOST_REQUIRE_NO_THROW(w_min.check_password(password));
  BOOST_REQUIRE_THROW(w_min.check_password("wrong"), std::runtime_error);
}

/// 12. Bulk import produces identical key sets
BOOST_AUTO_TEST_CASE(bulk_import_same_keys)
{
  memory_storage ms;

  // Generate some random keys
  std::vector<std::string> wif_keys;
  std::vector<fc::ecc::public_key> expected_pubs;
  for (int i = 0; i < 10; ++i)
  {
    auto priv = fc::ecc::private_key::generate();
    wif_keys.push_back(priv.key_to_wif());
    expected_pubs.push_back(priv.get_public_key());
  }

  // ── original ──
  beekeeper::wallet_content_handler original(true);
  original.set_password(password);
  original.unlock(password);
  for (auto& wif : wif_keys)
    original.import_key(wif, prefix);
  auto orig_keys = original.get_keys_details();

  // ── minimal ──
  beekeeper_minimal::wallet w_min(crypto, &ms, "bulk_test");
  w_min.create(password);
  for (auto& wif : wif_keys)
    w_min.import_key(wif, prefix);
  auto& min_keys = w_min.get_keys();

  BOOST_REQUIRE_EQUAL(orig_keys.size(), min_keys.size());
  BOOST_REQUIRE_EQUAL(orig_keys.size(), 10u);

  for (auto& [fc_pub, fc_data] : orig_keys)
  {
    auto min_pub = to_min_pubkey(fc_pub);
    auto it = min_keys.find(min_pub);
    BOOST_REQUIRE(it != min_keys.end());
    BOOST_REQUIRE_EQUAL(crypto.key_to_wif(it->second.first), fc_data.first.key_to_wif());
    BOOST_REQUIRE_EQUAL(it->second.second, fc_data.second);
  }
}

/// 13. Multiple sign_digest calls on different keys produce matching signatures
BOOST_AUTO_TEST_CASE(multi_key_sign_digest)
{
  memory_storage ms;

  auto priv1 = fc::ecc::private_key::wif_to_key(wif_key1).value();
  auto priv2 = fc::ecc::private_key::wif_to_key(wif_key2).value();
  auto priv3 = fc::ecc::private_key::wif_to_key(wif_key3).value();

  auto digest1 = fc::sha256::hash("first transaction");
  auto digest2 = fc::sha256::hash("second transaction");

  // ── original ──
  beekeeper::wallet_content_handler original(true);
  original.set_password(password);
  original.unlock(password);
  original.import_key(wif_key1, prefix);
  original.import_key(wif_key2, prefix);
  original.import_key(wif_key3, prefix);

  // ── minimal ──
  beekeeper_minimal::wallet w_min(crypto, &ms, "multi_sign");
  w_min.create(password);
  w_min.import_key(wif_key1, prefix);
  w_min.import_key(wif_key2, prefix);
  w_min.import_key(wif_key3, prefix);

  // Sign each digest with each key and compare
  struct test_case { fc::sha256 digest; fc::ecc::public_key pub; };
  std::vector<test_case> cases = {
    { digest1, priv1.get_public_key() },
    { digest1, priv2.get_public_key() },
    { digest1, priv3.get_public_key() },
    { digest2, priv1.get_public_key() },
    { digest2, priv2.get_public_key() },
    { digest2, priv3.get_public_key() },
  };

  for (auto& tc : cases)
  {
    auto sig_orig = original.try_sign_digest(tc.digest, tc.pub);
    auto min_pub = to_min_pubkey(tc.pub);
    auto min_dig = to_min_digest(tc.digest);
    auto sig_min  = w_min.try_sign_digest(min_dig, min_pub);
    BOOST_REQUIRE(sig_orig.has_value());
    BOOST_REQUIRE(sig_min.has_value());
    BOOST_REQUIRE_EQUAL(fc_sig_to_hex(*sig_orig), min_sig_to_hex(*sig_min));
  }
}

/// 14. Key not in wallet → try_sign_digest returns nullopt in both
BOOST_AUTO_TEST_CASE(sign_missing_key_returns_nullopt)
{
  memory_storage ms;

  auto priv1 = fc::ecc::private_key::wif_to_key(wif_key1).value();
  auto pub1  = priv1.get_public_key();
  auto priv3 = fc::ecc::private_key::wif_to_key(wif_key3).value();
  auto pub3  = priv3.get_public_key();
  auto digest = fc::sha256::hash("test");

  auto min_pub1 = to_min_pubkey(pub1);
  auto min_pub3 = to_min_pubkey(pub3);
  auto min_digest = to_min_digest(digest);

  // Only import key1
  beekeeper::wallet_content_handler original(true);
  original.set_password(password);
  original.unlock(password);
  original.import_key(wif_key1, prefix);

  beekeeper_minimal::wallet w_min(crypto, &ms, "missing_key");
  w_min.create(password);
  w_min.import_key(wif_key1, prefix);

  // key3 not imported → nullopt
  BOOST_REQUIRE(!original.try_sign_digest(digest, pub3).has_value());
  BOOST_REQUIRE(!w_min.try_sign_digest(min_digest, min_pub3).has_value());

  // key1 is present → has value
  BOOST_REQUIRE(original.try_sign_digest(digest, pub1).has_value());
  BOOST_REQUIRE(w_min.try_sign_digest(min_digest, min_pub1).has_value());
}

// ─────────────────────────────────────────────────────────────
BOOST_AUTO_TEST_SUITE_END()
// ─────────────────────────────────────────────────────────────
