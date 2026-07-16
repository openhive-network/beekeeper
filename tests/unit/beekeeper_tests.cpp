#ifdef IS_TEST_NET

#define BOOST_TEST_MODULE beekeeper_tests

#include "beekeeper_mgr.hpp"

#ifdef HIVE_PROTOCOL_AVAILABLE
#include <hive/protocol/hive_operations.hpp>
#include <hive/protocol/transaction.hpp>
#endif

#include <boost/test/included/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/filesystem.hpp>
#include <fc/io/json.hpp>
#include <fc/crypto/crypto_data.hpp>

#include <beekeeper/extended_api.hpp>

#include <cstring>
#include <thread>
#include <condition_variable>

using beekeeper_type = beekeeper_minimal::beekeeper;
using keys_map       = beekeeper_minimal::keys_map;

BOOST_AUTO_TEST_SUITE(beekeeper_tests)

/// Test basic wallet operations via beekeeper
BOOST_AUTO_TEST_CASE(wallet_test)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  auto _prefix = "STM";

  // Create a wallet and import a key
  auto _pw = bk.create_wallet(_token, "test", "pass", false);
  BOOST_REQUIRE(!bk.list_wallets(_token).empty());

  auto priv = fc::ecc::private_key::generate();
  auto wif = priv.key_to_wif();

  auto pub_str = bk.import_key("test", wif, _prefix);
  auto keys = bk.get_public_keys("test");
  BOOST_REQUIRE_EQUAL(1u, keys.size());

  // Verify the imported key round-trips
  auto min_pub = b_mgr.crypto.public_key_from_string(pub_str, _prefix);
  BOOST_REQUIRE(keys.count(min_pub) > 0);
  auto& stored_priv = keys.begin()->second.first;
  BOOST_REQUIRE_EQUAL(wif, b_mgr.crypto.key_to_wif(stored_priv));

  // Lock and unlock
  bk.lock("test");
  BOOST_REQUIRE_THROW(bk.get_public_keys("test"), std::exception);
  bk.unlock("test", "pass");
  keys = bk.get_public_keys("test");
  BOOST_REQUIRE_EQUAL(1u, keys.size());

  // Close and reopen from storage
  bk.close_wallet("test");
  bk.open_wallet(_token, "test");
  bk.unlock("test", "pass");
  keys = bk.get_public_keys("test");
  BOOST_REQUIRE_EQUAL(1u, keys.size());
  BOOST_REQUIRE_EQUAL(wif, b_mgr.crypto.key_to_wif(keys.begin()->second.first));

  // Bulk import
  std::vector<std::string> wifs;
  for (int i = 0; i < 4; ++i)
    wifs.push_back(fc::ecc::private_key::generate().key_to_wif());

  for( const auto& w : wifs )
    bk.import_key("test", w, _prefix);
  keys = bk.get_public_keys("test");
  BOOST_REQUIRE_EQUAL(5u, keys.size());

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(wallet_name_test)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  bk.create_wallet(_token, "wallet.wallet", "", false);
  bk.create_wallet(_token, "wallet_wallet", "", false);
  bk.create_wallet(_token, "wallet-wallet", "", false);
  bk.create_wallet(_token, "wallet@wallet", "", false);

  bk.create_wallet(_token, ".wallet", "", false);
  bk.create_wallet(_token, "_wallet", "", false);
  bk.create_wallet(_token, "-wallet", "", false);
  bk.create_wallet(_token, "@wallet", "", false);

  bk.create_wallet(_token, "wallet.", "", false);
  bk.create_wallet(_token, "wallet_", "", false);
  bk.create_wallet(_token, "wallet-", "", false);
  bk.create_wallet(_token, "wallet@", "", false);

  bk.create_wallet(_token, ".wallet.", "", false);
  bk.create_wallet(_token, "_wallet_", "", false);
  bk.create_wallet(_token, "-wallet-", "", false);
  bk.create_wallet(_token, "@wallet@", "", false);

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(wallet_complex_name_test)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  std::string _wallet_name = "small.minion.wallet";

  bk.create_wallet(_token, _wallet_name, "", false);

  auto _wallets = bk.list_wallets(_token);
  // Wallet appears in list (from session + storage scan)
  bool found = false;
  for (auto& w : _wallets)
    if (w.name == _wallet_name) { found = true; break; }
  BOOST_REQUIRE(found);

  bk.lock(_wallet_name);

  // After lock, wallet still appears but as locked
  _wallets = bk.list_wallets(_token);
  for (auto& w : _wallets)
    if (w.name == _wallet_name)
      BOOST_REQUIRE(!w.unlocked);

} FC_LOG_AND_RETHROW() }

/// Test wallet manager key operations
BOOST_AUTO_TEST_CASE(wallet_manager_test)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  const auto key1_str = "5JktVNHnRX48BUdtewU7N1CyL4Z886c42x7wYW7XhNWkDQRhdcS";
  const auto key2_str = "5Ju5RTcVDo35ndtzHioPMgebvBM6LkJ6tvuU6LTNQv8yaz3ggZr";
  const auto key3_str = "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3";

  auto _prefix = "STM";

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  // No wallets initially (no .wallet files on disk)
  BOOST_REQUIRE_EQUAL(0u, bk.list_wallets(_token).size());

  // Operations on non-existent wallets should throw
  BOOST_REQUIRE_THROW(bk.get_public_keys("avocado"), std::exception);
  bk.lock_all(); // no-op, no wallets

  BOOST_REQUIRE_THROW(bk.lock("test"), std::exception);
  BOOST_REQUIRE_THROW(bk.unlock("test", "pw"), std::exception);
  BOOST_REQUIRE_THROW(bk.import_key("test", "pw", _prefix), std::exception);

  auto pw = bk.create_wallet(_token, "test", "", false);
  BOOST_REQUIRE(!pw.empty());
  BOOST_REQUIRE_EQUAL(0u, pw.find("PW")); // starts with PW

  // wallet has no keys when it is created
  BOOST_REQUIRE_EQUAL(0u, bk.get_public_keys("test").size());

  bk.lock("test");
  // find "test" wallet in list, check it's locked
  {
    auto wallets = bk.list_wallets(_token);
    bool found = false;
    for (auto& w : wallets)
      if (w.name == "test") { BOOST_REQUIRE(!w.unlocked); found = true; }
    BOOST_REQUIRE(found);
  }
  bk.unlock("test", pw);
  BOOST_REQUIRE_THROW(bk.unlock("test", pw), std::exception); // already unlocked

  auto pub1_str = bk.import_key("test", key1_str, _prefix);
  BOOST_REQUIRE_EQUAL(1u, bk.get_public_keys("test").size());

  auto keys = bk.get_public_keys("test");
  auto min_pub1 = b_mgr.crypto.public_key_from_string(pub1_str, _prefix);
  BOOST_REQUIRE(keys.count(min_pub1) > 0);

  auto pub2_str = bk.import_key("test", key2_str, _prefix);
  keys = bk.get_public_keys("test");
  auto min_pub2 = b_mgr.crypto.public_key_from_string(pub2_str, _prefix);
  BOOST_REQUIRE(keys.count(min_pub1) > 0);
  BOOST_REQUIRE(keys.count(min_pub2) > 0);
  // key3 was not automatically imported
  auto min_priv3 = b_mgr.crypto.wif_to_key(key3_str);
  BOOST_REQUIRE(min_priv3.has_value());
  auto min_pub3 = b_mgr.crypto.get_public_key(*min_priv3);
  BOOST_REQUIRE(keys.count(min_pub3) == 0);

  bk.remove_key("test", min_pub2);
  BOOST_REQUIRE_EQUAL(1u, bk.get_public_keys("test").size());
  keys = bk.get_public_keys("test");
  BOOST_REQUIRE(keys.count(min_pub2) == 0);

  bk.import_key("test", key2_str, _prefix);
  BOOST_REQUIRE_EQUAL(2u, bk.get_public_keys("test").size());
  keys = bk.get_public_keys("test");
  BOOST_REQUIRE(keys.count(min_pub2) > 0);

  BOOST_REQUIRE_THROW(bk.remove_key("test", min_pub3), std::exception);
  BOOST_REQUIRE_EQUAL(2u, bk.get_public_keys("test").size());
  BOOST_REQUIRE_THROW(bk.remove_key("test_xyz", min_pub2), std::exception);

  bk.lock("test");
  BOOST_REQUIRE_THROW(bk.get_public_keys("test"), std::exception);
  bk.unlock("test", pw);
  BOOST_REQUIRE_EQUAL(2u, bk.get_public_keys("test").size());
  bk.lock_all();
  BOOST_REQUIRE_THROW(bk.get_public_keys("test"), std::exception);

  auto pw2 = bk.create_wallet(_token, "test2", "", false);
  // wallet has no keys when it is created
  BOOST_REQUIRE_EQUAL(0u, bk.get_public_keys("test2").size());
  bk.import_key("test2", key3_str, _prefix);
  BOOST_REQUIRE_EQUAL(1u, bk.get_public_keys("test2").size());
  bk.import_key("test2", key3_str, _prefix); // duplicate import
  keys = bk.get_public_keys("test2");
  BOOST_REQUIRE(keys.count(min_pub1) == 0);
  BOOST_REQUIRE(keys.count(min_pub2) == 0);
  BOOST_REQUIRE(keys.count(min_pub3) > 0);

  bk.unlock("test", pw);
  auto keys_test = bk.get_public_keys("test");
  auto keys_test2 = bk.get_public_keys("test2");
  BOOST_REQUIRE_EQUAL(keys_test.size() + keys_test2.size(), 3u);

  bk.lock_all();
  BOOST_REQUIRE_THROW(bk.get_public_keys("test"), std::exception);
  BOOST_REQUIRE_THROW(bk.get_public_keys("test2"), std::exception);

  // Recreate wallet after file deletion
  bk.close_wallet("testgen"); // no-op if doesn't exist
  bk.create_wallet(_token, "testgen", "", false);
  bk.lock("testgen");
  bk.close_wallet("testgen");
  fc::remove( b_mgr.dir / "testgen.wallet" );

  pw = bk.create_wallet(_token, "testgen", "", false);
  bk.lock("testgen");
  BOOST_REQUIRE(fc::exists( b_mgr.dir / "testgen.wallet" ));

} FC_LOG_AND_RETHROW() }

/// Test wallet manager create with invalid names
BOOST_AUTO_TEST_CASE(wallet_manager_create_test)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();
    auto _prefix = "STM";

    bk.create_wallet(_token, "test", "", false);
    constexpr auto key1 = "5JktVNHnRX48BUdtewU7N1CyL4Z886c42x7wYW7XhNWkDQRhdcS";
    bk.import_key("test", key1, _prefix);
    BOOST_REQUIRE_THROW(bk.create_wallet(_token, "test", "", false),        std::exception);

    bk.create_wallet(_token, ".test", "", false);
    BOOST_REQUIRE(fc::exists( b_mgr.dir / ".test.wallet" ));

    bk.create_wallet(_token, "..test", "", false);
    BOOST_REQUIRE(fc::exists( b_mgr.dir / "..test.wallet" ));

    bk.create_wallet(_token, "...test", "", false);
    BOOST_REQUIRE(fc::exists( b_mgr.dir / "...test.wallet" ));

    bk.create_wallet(_token, ".", "", false);
    BOOST_REQUIRE(fc::exists( b_mgr.dir / "..wallet" ));

    bk.create_wallet(_token, "__test_test", "", false);
    BOOST_REQUIRE(fc::exists( b_mgr.dir / "__test_test.wallet" ));

    bk.create_wallet(_token, "t-t", "", false);
    BOOST_REQUIRE(fc::exists( b_mgr.dir / "t-t.wallet" ));

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_sessions)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    {
      auto bk = b_mgr.create_beekeeper();
      auto _token = bk.create_session();
      bk.close_session( _token );
      // Session is closed — token should be invalid
      BOOST_REQUIRE_THROW(bk.validate_token(_token), std::exception);
    }
    {
      auto bk = b_mgr.create_beekeeper();
      b_mgr.remove_wallets();

      auto _token_00 = bk.create_session();
      auto _token_01 = bk.create_session();

      bk.create_wallet(_token_00, "avocado", "", false);
      bk.create_wallet(_token_01, "banana", "", false);
      bk.create_wallet(_token_01, "cherry", "", false);

      BOOST_REQUIRE_THROW( bk.validate_token( "not existed token" ), std::exception );

      // Session-scoped wallet visibility
      auto wallets_00 = bk.list_wallets( _token_00 );
      auto wallets_01 = bk.list_wallets( _token_01 );
      // Both sessions see all wallets (session + storage)
      // token_00 has "avocado" in session, token_01 has "banana" and "cherry"
      // Both also see all 3 from storage

      bk.close_session( _token_00 );

      BOOST_REQUIRE_THROW( bk.validate_token( "not existed token" ), std::exception );
      BOOST_REQUIRE_THROW( bk.validate_token( _token_00 ), std::exception );
      // token_01 still valid
      BOOST_REQUIRE_NO_THROW( bk.validate_token( _token_01 ) );

      bk.close_session( _token_01 );

      BOOST_REQUIRE_THROW( bk.validate_token( _token_00 ), std::exception );
      BOOST_REQUIRE_THROW( bk.validate_token( _token_01 ), std::exception );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_wallets_with_dots)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    bk.create_wallet(_token, "...watermelon", "", false);
    bk.create_wallet(_token, ".lemon", "", false);
    bk.create_wallet(_token, ".peach.pear", "", false);
    bk.create_wallet(_token, ".plum.", "", false);
    bk.create_wallet(_token, "avocado.banana", "", false);

    BOOST_REQUIRE_EQUAL( b_mgr.storage->list_dir().size(), 5 );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_info)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    auto _info = bk.get_info();

    BOOST_REQUIRE( !_info.now.empty() );
    BOOST_REQUIRE( !_info.timeout_time.empty() );

    // Timeout should be in the future
    BOOST_TEST_MESSAGE( "now: " + _info.now + " timeout: " + _info.timeout_time );
    BOOST_REQUIRE( _info.timeout_time > _info.now );

    bk.close_session( _token );
    BOOST_REQUIRE_THROW( bk.validate_token( _token ), std::exception );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallet_manager_close)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto bk = b_mgr.create_beekeeper();

    auto wallet_name_0 = "0";
    auto wallet_name_1 = "1";

    {
      auto _token = bk.create_session();
      bk.create_wallet(_token, wallet_name_0, "", false);

      auto _wallets = bk.list_wallets( _token );
      // "0" is in session and on disk
      bool found_0 = false;
      for (auto& w : _wallets)
        if (w.name == wallet_name_0 && w.unlocked) found_0 = true;
      BOOST_REQUIRE(found_0);

      bk.close_wallet( wallet_name_1 ); // non-existent — no error (silent)
      bk.close_wallet( wallet_name_0 );

      // After close, wallet "0" still appears from storage (as locked)
      _wallets = bk.list_wallets( _token );
      for (auto& w : _wallets)
        if (w.name == wallet_name_0) BOOST_REQUIRE(!w.unlocked);

      // Remove file so we start fresh for wallet_name_1
      b_mgr.remove_wallet(wallet_name_0);
      bk.create_wallet(_token, wallet_name_1, "", false);

      _wallets = bk.list_wallets( _token );
      bool found_1 = false;
      for (auto& w : _wallets)
        if (w.name == wallet_name_1 && w.unlocked) found_1 = true;
      BOOST_REQUIRE(found_1);

      bk.close_wallet( wallet_name_1 );
    }

    {
      b_mgr.remove_wallets();

      auto _token = bk.create_session();
      bk.create_wallet(_token, wallet_name_0, "", false);

      bk.lock_all();

      auto _wallets = bk.list_wallets( _token );
      bool found_0 = false;
      for (auto& w : _wallets)
        if (w.name == wallet_name_0) { BOOST_REQUIRE(!w.unlocked); found_0 = true; }
      BOOST_REQUIRE(found_0);

      bk.close_wallet( wallet_name_0 );
    }

    {
      b_mgr.remove_wallets();

      auto _token = bk.create_session();
      bk.create_wallet(_token, wallet_name_0, "", false);
      bk.create_wallet(_token, wallet_name_1, "", false);

      bk.lock( wallet_name_1 );

      auto _wallets = bk.list_wallets( _token );
      size_t _count = 0;
      for (auto& w : _wallets)
      {
        if (w.name == wallet_name_0) { BOOST_REQUIRE(w.unlocked); ++_count; }
        if (w.name == wallet_name_1) { BOOST_REQUIRE(!w.unlocked); ++_count; }
      }
      BOOST_REQUIRE_EQUAL( _count, 2u );

      bk.close_wallet( wallet_name_1 );

      _wallets = bk.list_wallets( _token );
      bool found_0 = false;
      for (auto& w : _wallets)
        if (w.name == wallet_name_0 && w.unlocked) found_0 = true;
      BOOST_REQUIRE(found_0);

      bk.lock( wallet_name_0 );
      bk.close_wallet( wallet_name_0 );
    }

  } FC_LOG_AND_RETHROW()
}

#ifdef HIVE_PROTOCOL_AVAILABLE
BOOST_AUTO_TEST_CASE(wallet_manager_sign_transaction)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    {
      hive::protocol::serialization_mode_controller::pack_guard guard( hive::protocol::pack_type::hf26 );

      auto _private_key_str = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n";
      auto _public_key_str  = "6LLegbAgLAy28EHrffBVuANFWcFgmqRMW13wBmTExqFE9SCkg4";

      const std::string _wallet_name = "0";
      auto _prefix = "STM";

      auto bk = b_mgr.create_beekeeper();
      auto _token = bk.create_session();
      auto _password = bk.create_wallet(_token, _wallet_name, "", false);
      auto _imported_public_key = bk.import_key( _wallet_name, _private_key_str, _prefix );
      BOOST_REQUIRE( _imported_public_key != _public_key_str );
      BOOST_REQUIRE( _imported_public_key == std::string( HIVE_ADDRESS_PREFIX ) + _public_key_str );

      auto _calculate_signature = [&]( const std::string& json_trx, const std::string& signature_pattern )
      {
        hive::protocol::transaction _trx = fc::json::from_string( json_trx, fc::json::format_validation_mode::full ).as<hive::protocol::transaction>();
        hive::protocol::digest_type _sig_digest = _trx.sig_digest( HIVE_CHAIN_ID, hive::protocol::pack_type::hf26 );

        auto _private_key = fc::ecc::private_key::wif_to_key( _private_key_str ).value();
        auto _signature_local   = _private_key.sign_compact( _sig_digest );
        auto __signature_local  = fc::json::to_string( _signature_local );

        auto min_digest = b_mgr.crypto.digest_from_hex( _sig_digest.str() );
        auto min_pub = b_mgr.crypto.public_key_from_string( _imported_public_key, _prefix );
        auto _signature_wallet  = bk.sign_digest( _wallet_name, min_digest, min_pub, _prefix );
        auto __signature_wallet = b_mgr.crypto.signature_to_hex( _signature_wallet );

        BOOST_TEST_MESSAGE( __signature_local );
        BOOST_REQUIRE( __signature_local.substr( 1, __signature_local.size() - 2 )    == signature_pattern );

        BOOST_TEST_MESSAGE( __signature_wallet );
        BOOST_REQUIRE( __signature_wallet == signature_pattern );
      };

      std::string _signature_00_result = "1f17cc07f7c769073d39fac3385220b549e261fb33c5f619c5dced7f5b0fe9c0954f2684e703710840b7ea01ad7238b8db1d8a9309d03e93de212f86de38d66f21";
      _calculate_signature( "{}", _signature_00_result );

      std::string _signature_01_result = "1f69e091fc79b0e8d1812fc662f12076561f9e38ffc212b901ae90fe559f863ad266fe459a8e946cff9bbe7e56ce253bbfab0cccdde944edc1d05161c61ae86340";
      _calculate_signature( "{\"ref_block_num\":95,\"ref_block_prefix\":4189425605,\"expiration\":\"2023-07-18T08:38:29\",\"operations\":[{\"type\":\"transfer_operation\",\"value\":{\"from\":\"initminer\",\"to\":\"alice\",\"amount\":{\"amount\":\"666\",\"precision\":3,\"nai\":\"@@000000021\"},\"memo\":\"memmm\"}}],\"extensions\":[],\"signatures\":[],\"transaction_id\":\"cc9630cdbc39da1c9b6264df3588c7bedb5762fa\",\"block_num\":0,\"transaction_num\":0}",
                            _signature_01_result );

      auto min_pub = b_mgr.crypto.public_key_from_string( _imported_public_key, _prefix );
      BOOST_REQUIRE_THROW( bk.sign_digest( _wallet_name, b_mgr.crypto.digest_from_hex(""), min_pub, _prefix ), std::exception );
    }

  } FC_LOG_AND_RETHROW()
}
#endif // HIVE_PROTOCOL_AVAILABLE

BOOST_AUTO_TEST_CASE(wallet_manager_brute_force_protection_test)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    uint64_t _interval = 10;
    beekeeper::extended_api _api( _interval );

    const uint32_t _nr_threads = 10;

    auto _unlock_in_threads = [&]()
    {
      std::atomic<bool> _ready{false};
      std::mutex _mtx_message;
      std::mutex _mtx;
      std::condition_variable _cv;

      std::vector<std::shared_ptr<std::thread>> threads;

      auto _start = std::chrono::high_resolution_clock::now();

      auto _calculate_interval = [&_start]( size_t number_thread )
      {
        std::string _message = "*****thread: " + std::to_string( number_thread ) + " *****";
        auto _duration = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - _start );
        BOOST_TEST_MESSAGE( _message + std::to_string( _duration.count() ) + " [ms]" );

        return _duration;
      };

      auto _calculate_summary = [&_start]()
      {
        auto _duration = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - _start );
        BOOST_TEST_MESSAGE( std::to_string( _duration.count() ) + " [ms]" );

        return _duration;
      };

      std::atomic<size_t> _cnt{0};

      for( size_t i = 0; i < _nr_threads; ++i )
        threads.emplace_back( std::make_shared<std::thread>( [&]( size_t number_thread )
        {
          {
            std::unique_lock<std::mutex> _lock( _mtx );
            _cv.wait( _lock, [&_ready](){ return true; } );
          }

          if( number_thread % 2 == 0 )
          {
            while( _cnt.load() < _nr_threads / 2 )
            {
              _api.was_error();
              std::this_thread::sleep_for( std::chrono::milliseconds(1) );
            }
          }
          else
          {
            while( _api.unlock_allowed() != beekeeper::extended_api::enabled_after_interval )
            {
              std::this_thread::sleep_for( std::chrono::milliseconds(1) );
            }
            {
              std::lock_guard<std::mutex> _guard( _mtx_message );
              _calculate_interval( number_thread / 2 );
              _cnt.store( _cnt.load() + 1 );
            }
          }
        }, i ) );

      std::this_thread::sleep_for( std::chrono::milliseconds(10) );
      _ready.store( true );
      _cv.notify_all();

      for( auto& thread : threads )
        thread->join();

      return _calculate_summary();
    };

    const uint32_t _nr_attempts = 20;
    bool _work_in_threads_was_correct = false;
    for( uint32_t i = 0; i < _nr_attempts; ++i )
    {
      auto _duration = _unlock_in_threads();
      if( _duration.count() >= (int64_t)( _interval * ( _nr_threads / 2 ) ) )
      {
        BOOST_TEST_MESSAGE("********unlocks work correctly in many threads. Finished: (" + std::to_string(i) + ")");
        _work_in_threads_was_correct = true;
        break;
      }
      else
      {
        BOOST_TEST_MESSAGE("********unlocks didn't work correctly in many threads. Repeating: (" + std::to_string(i) + ")");
      }
    }
    BOOST_REQUIRE( _work_in_threads_was_correct );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(beekeeper_timeout)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    // Create a few wallets
    bk.create_wallet(_token, "w0", "", false);
    bk.create_wallet(_token, "w1", "", false);

    // Set a 1-second timeout
    bk.set_timeout(1);

    // Wallets should be unlocked right now
    {
      auto wallets = bk.list_wallets(_token);
      for (auto& w : wallets)
        if (w.name == "w0" || w.name == "w1")
          BOOST_REQUIRE(w.unlocked);
    }

    // Wait for timeout to fire
    std::this_thread::sleep_for( std::chrono::milliseconds(1200) );
    bk.check_timeout();

    // All wallets should be locked now
    {
      auto wallets = bk.list_wallets(_token);
      for (auto& w : wallets)
        if (w.name == "w0" || w.name == "w1")
          BOOST_REQUIRE(!w.unlocked);
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(beekeeper_refresh_timeout)
{
  try {
    const std::string _wallet_name = "0";

    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();
    auto _password = bk.create_wallet(_token, "0", "", false);
    bk.set_timeout(1);

    // Keep refreshing timeout by calling operations
    for( uint32_t i = 0; i < 12; ++i )
    {
      std::this_thread::sleep_for( std::chrono::milliseconds(250) );
      try { bk.get_public_keys("0"); } catch(...) {}
    }

    // Wallet should still be unlocked because we kept refreshing
    auto wallets = bk.list_wallets(_token);
    bool found_unlocked = false;
    for (auto& w : wallets)
      if (w.name == "0" && w.unlocked) found_unlocked = true;
    BOOST_REQUIRE(found_unlocked);

  } FC_LOG_AND_RETHROW()
}

#ifdef HIVE_PROTOCOL_AVAILABLE
BOOST_AUTO_TEST_CASE(has_matching_private_key_endpoint_test)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto _private_key_str = "5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n";
    auto _private_key_str_2 = "5J8C7BMfvMFXFkvPhHNk2NHGk4zy3jF4Mrpf5k5EzAecuuzqDnn";

    auto _prefix = "ABC";

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();
    auto _password = bk.create_wallet(_token, "0", "", false);

    auto pub_str = bk.import_key( "0", _private_key_str, _prefix );
    auto min_pub = b_mgr.crypto.public_key_from_string(pub_str, _prefix);

    auto pub_str_2 = fc::ecc::private_key::wif_to_key(_private_key_str_2).value().get_public_key();
    auto min_pub_2_str = _prefix + fc::ecc::public_key::to_base58(pub_str_2, false);
    auto min_pub_2 = b_mgr.crypto.public_key_from_string(min_pub_2_str, _prefix);

    BOOST_REQUIRE_THROW( bk.has_private_key( "pear", min_pub ), std::exception );

    BOOST_REQUIRE_EQUAL( bk.has_private_key( "0", min_pub ), true );
    BOOST_REQUIRE_EQUAL( bk.has_private_key( "0", min_pub_2 ), false );

    bk.import_key( "0", _private_key_str_2, _prefix );

    BOOST_REQUIRE_EQUAL( bk.has_private_key( "0", min_pub ), true );
    BOOST_REQUIRE_EQUAL( bk.has_private_key( "0", min_pub_2 ), true );

    bk.close_wallet( "0" );

    BOOST_REQUIRE_THROW( bk.has_private_key( "0", min_pub ), std::exception );

  } FC_LOG_AND_RETHROW()
}
#endif // HIVE_PROTOCOL_AVAILABLE

BOOST_AUTO_TEST_CASE(beekeeper_timeout_unlock)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    struct wallet_info
    {
      std::string name;
      std::string password;
    };
    std::vector<wallet_info> _wallets{ { "0", "" }, { "1", "" } };

    for( auto& w : _wallets )
    {
      w.password = bk.create_wallet( _token, w.name, "", false );
    }
    {
      bk.set_timeout( 1 );
      std::this_thread::sleep_for( std::chrono::milliseconds(1200) );
      bk.check_timeout();
    }
    {
      for( auto& w : _wallets )
      {
        bk.unlock( w.name, w.password );
      }
    }
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(beekeeper_timeout_list_wallets)
{
  try {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    struct wallet_info
    {
      std::string name;
      std::string password;
    };
    std::vector<wallet_info> _wallets{ { "0", "" }, { "1", "" }, { "2", "" } };

    for( auto& w : _wallets )
    {
      w.password = bk.create_wallet( _token, w.name, "", false );
      bk.close_wallet( w.name );
    }

    // After closing all wallets, they should be listed as locked (from storage)
    {
      auto listed = bk.list_wallets( _token );
      for (auto& lw : listed)
        BOOST_REQUIRE(!lw.unlocked);
    }

    // Unlock wallet "1"
    {
      bk.open_wallet( _token, _wallets[1].name );
      bk.unlock( _wallets[1].name, _wallets[1].password );
    }
    {
      bk.set_timeout( 1 );
      std::this_thread::sleep_for( std::chrono::milliseconds(1200) );
      bk.check_timeout();
    }
    {
      // After timeout, wallet "1" should be locked
      auto listed = bk.list_wallets( _token );
      for (auto& w : listed)
        if (w.name == _wallets[1].name)
          BOOST_REQUIRE(!w.unlocked);
    }
    // Unlock wallet "1" again
    {
      bk.unlock( _wallets[1].name, _wallets[1].password );
    }
    {
      auto listed = bk.list_wallets( _token );
      for (auto& w : listed)
        if (w.name == _wallets[1].name)
          BOOST_REQUIRE(w.unlocked);
    }
    {
      std::this_thread::sleep_for( std::chrono::milliseconds(1200) );
      bk.check_timeout();
    }
    {
      auto listed = bk.list_wallets( _token );
      for (auto& w : listed)
        if (w.name == _wallets[1].name)
          BOOST_REQUIRE(!w.unlocked);
    }
  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(data_reliability_when_file_with_wallet_is_removed)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto _prefix = "STM";

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    struct keys_item
    {
      std::string private_key;
    };
    std::vector<keys_item> _keys_a =
    {
      {"5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze"},
      {"5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su"},
      {"5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw"},
      {"5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG"},
      {"5KKvoNaCPtN9vUEU1Zq9epSAVsEPEtocbJsp7pjZndt9Rn4dNRg"}
    };
    std::vector<keys_item> _keys_b =
    {
      {"5JkFnXrLM2ap9t3AmAxBJvQHF7xSKtnTrCTginQCkhzU5S7ecPT"},
      {"5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78"},
      {"5KNbAE7pLwsLbPUkz6kboVpTR24CycqSNHDG95Y8nbQqSqd6tgS"},
      {"5JNHfZYKGaomSFvd4NUdQ9qMcEAC43kujbfjueTHpVapX1Kzq2n"},
      {"5J8C7BMfvMFXFkvPhHNk2NHGk4zy3jF4Mrpf5k5EzAecuuzqDnn"}
    };

    struct wallet_info
    {
      std::string name;
      std::string password;
    };
    std::vector<wallet_info> _wallets{ { "0", "" }, { "1", "" }, { "2", "" } };

    for( auto& w : _wallets )
    {
      w.password = bk.create_wallet( _token, w.name, "", false );
      for( auto& item : _keys_a )
        bk.import_key( w.name, item.private_key, _prefix );
    }

    b_mgr.remove_wallet( _wallets[0].name );
    b_mgr.remove_wallet( _wallets[1].name );

    {
      // Wallet "0" is still in memory even though file was deleted
      for( auto& item : _keys_a )
        bk.import_key( _wallets[0].name, item.private_key, _prefix );

      auto _public_keys_0 = bk.get_public_keys( _wallets[0].name );
      auto _public_keys_2 = bk.get_public_keys( _wallets[2].name );
      BOOST_REQUIRE_EQUAL( _public_keys_0.size(), 5 );
      BOOST_REQUIRE_EQUAL( _public_keys_2.size(), 5 );
    }
    {
      for( auto& item : _keys_b )
      {
        bk.import_key( _wallets[1].name, item.private_key, _prefix );
        bk.import_key( _wallets[2].name, item.private_key, _prefix );
      }

      auto _public_keys_1 = bk.get_public_keys( _wallets[1].name );
      auto _public_keys_2 = bk.get_public_keys( _wallets[2].name );
      BOOST_REQUIRE_EQUAL( _public_keys_1.size(), 10 );
      BOOST_REQUIRE_EQUAL( _public_keys_2.size(), 10 );
    }

    BOOST_REQUIRE( !b_mgr.exists_wallet( _wallets[0].name ) );
    BOOST_REQUIRE( b_mgr.exists_wallet( _wallets[1].name ) );
    BOOST_REQUIRE( b_mgr.exists_wallet( _wallets[2].name ) );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(encrypt_decrypt_data)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto _prefix = "STM";

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    struct key_pair
    {
      std::string private_key;
      std::string public_key;
    };
    std::vector<key_pair> _keys =
    {
      {"5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze", "STM6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW"},
      {"5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su", "STM8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME"},
      {"5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw", "STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4"},
      {"5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG", "STM6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45"}
    };

    const std::string _fruits_content = "avocado-banana-cherry-durian";
    const std::string _empty_content = "";

    auto _to_min_pub = [&](uint32_t idx) {
      return b_mgr.crypto.public_key_from_string(_keys[idx].public_key, _prefix);
    };

    auto _encrypt_with_nonce = [&]( uint32_t from, uint32_t to, const std::string& wallet_name, const std::string& content, uint64_t nonce )
    {
      return bk.encrypt_data( wallet_name, _to_min_pub(from), _to_min_pub(to), content, _prefix, nonce );
    };

    auto _encrypt = [&]( uint32_t from, uint32_t to, const std::string& wallet_name, const std::string& content )
    {
      auto __encrypt = [&]()
      {
        return bk.encrypt_data( wallet_name, _to_min_pub(from), _to_min_pub(to), content, _prefix, 0 );
      };

      std::string _encrypted_content = __encrypt();
      std::string _encrypted_content_2 = __encrypt();

      BOOST_REQUIRE( _encrypted_content != _encrypted_content_2 );

      return std::make_pair( _encrypted_content, _encrypted_content_2 );
    };

    auto _decrypt = [&]( const std::string& pattern, uint32_t from, uint32_t to, const std::string& wallet_name, const std::string& content )
    {
      std::string _decrypted = bk.decrypt_data( wallet_name, _to_min_pub(from), _to_min_pub(to), content, _prefix );
      BOOST_REQUIRE_EQUAL( _decrypted, pattern );
    };

    struct wallet_info
    {
      std::string name;
      std::string password;
    };
    std::vector<wallet_info> _wallets{ { "0", "" }, { "1", "" }, { "2", "" }, { "3", "" } };

    //========================Preparation========================
    auto _cnt = 0;
    for( auto& w : _wallets )
    {
      w.password = bk.create_wallet( _token, w.name, "", false );
      switch( _cnt )
      {
        case 0:
          bk.import_key( w.name, _keys[0].private_key, _prefix );
        break;
        case 1:
          bk.import_key( w.name, _keys[1].private_key, _prefix );
        break;
        case 2:
          bk.import_key( w.name, _keys[0].private_key, _prefix );
          bk.import_key( w.name, _keys[1].private_key, _prefix );
        break;
        case 3:
          bk.import_key( w.name, _keys[2].private_key, _prefix );
          bk.import_key( w.name, _keys[3].private_key, _prefix );
        break;
      }
      ++_cnt;
    }
    bk.lock_all();
    //========================End of preparation========================

    {
      //lack of unlocked wallets
      BOOST_REQUIRE_THROW( _encrypt( 0, 1, _wallets[0].name, _fruits_content ), std::exception );
      BOOST_REQUIRE_THROW( _encrypt( 0, 1, _wallets[0].name, _empty_content ), std::exception );
    }
    {
      //unlock wallet "0"
      bk.unlock( _wallets[0].name, _wallets[0].password );

      auto _encrypted_content = _encrypt( 0, 1, _wallets[0].name, _fruits_content );
      _decrypt( _fruits_content, 0, 1, _wallets[0].name, _encrypted_content.first );
      _decrypt( _fruits_content, 0, 1, _wallets[0].name, _encrypted_content.second );

      auto _encrypted_content_2 = _encrypt( 0, 1, _wallets[0].name, _empty_content );
      _decrypt( _empty_content, 0, 1, _wallets[0].name, _encrypted_content_2.first );
      _decrypt( _empty_content, 0, 1, _wallets[0].name, _encrypted_content_2.second );

      bk.lock_all();

      BOOST_REQUIRE_THROW( _decrypt( "", 0, 1, _wallets[0].name, _encrypted_content.first ), std::exception );
    }
    {
      //unlock wallet "1" — has key[1] but not key[0], so encrypt from key[0] should fail
      bk.unlock( _wallets[1].name, _wallets[1].password );

      BOOST_REQUIRE_THROW( _encrypt( 0, 1, _wallets[1].name, _fruits_content ), std::exception );

      bk.lock_all();
    }
    {
      //unlock wallet "2" — has both key[0] and key[1]
      bk.unlock( _wallets[2].name, _wallets[2].password );

      auto _encrypted_content = _encrypt( 0, 1, _wallets[2].name, _fruits_content );
      _decrypt( _fruits_content, 0, 1, _wallets[2].name, _encrypted_content.first );

      _encrypted_content = _encrypt( 0, 1, _wallets[2].name, _empty_content );
      _decrypt( _empty_content, 0, 1, _wallets[2].name, _encrypted_content.first );

      bk.lock_all();
    }
    {
      //unlock wallet "3" — has key[2] and key[3], NOT key[0] or key[1]
      bk.unlock( _wallets[3].name, _wallets[3].password );

      BOOST_REQUIRE_THROW( _encrypt( 0, 1, _wallets[3].name, _fruits_content ), std::exception );

      bk.lock_all();
    }
    {
      //unlock all wallets
      for( auto& w : _wallets )
        bk.unlock( w.name, w.password );

      auto _encrypted_content = _encrypt( 0, 1, _wallets[0].name, _fruits_content );
      _decrypt( _fruits_content, 0, 1, _wallets[2].name, _encrypted_content.first );

      _encrypted_content = _encrypt( 0, 1, _wallets[2].name, _empty_content );
      _decrypt( _empty_content, 0, 1, _wallets[0].name, _encrypted_content.first );

      //`from` key == `to` key
      _encrypted_content = _encrypt( 0, 0, _wallets[2].name, _empty_content );
      _decrypt( _empty_content, 0, 0, _wallets[0].name, _encrypted_content.first );
    }
    {
      //test with different nonce
      auto _777 = _encrypt_with_nonce( 0, 1, _wallets[0].name, _fruits_content, 777 );
      auto _777_2 = _encrypt_with_nonce( 0, 1, _wallets[0].name, _fruits_content, 777 );
      auto _888 = _encrypt_with_nonce( 0, 1, _wallets[0].name, _fruits_content, 888 );
      auto _999 = _encrypt_with_nonce( 0, 1, _wallets[0].name, _fruits_content, 999 );
      auto _empty_0 = _encrypt_with_nonce( 0, 1, _wallets[0].name, _fruits_content, 0 );
      auto _empty_1 = _encrypt_with_nonce( 0, 1, _wallets[0].name, _fruits_content, 0 );

      BOOST_REQUIRE( _777 == _777_2 );
      BOOST_REQUIRE( _777 != _888 );
      BOOST_REQUIRE( _888 != _999 );
      BOOST_REQUIRE( _empty_0 != _empty_1 );
    }

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(encrypt_decrypt_data_with_many_keys)
{
  try
  {
      const std::string _fruits_content  = "avocado-banana-cherry-durian";
      const std::string _hex_content     = "0123456789ABCDEFabcdef";
      const std::string _empty_content   = "";

      auto _different_keys = []( const std::string& content )
      {
        //Using the `crypto_data` class do encryption/decryption for many `from`, `to` keys.
        fc::crypto_data _cd;

        for( auto i = 0; i < 1000; ++i )
        {
          auto _private_from  = fc::ecc::private_key::generate();
          auto _private_to    = fc::ecc::private_key::generate();

          auto _public_from   = _private_from.get_public_key();
          auto _public_to     = _private_to.get_public_key();

          std::string _encrypted_content = _cd.encrypt( _private_from, _public_to, content );

          auto _private_key_finder_from = [&_public_from, &_private_from]( const fc::crypto_data::public_key_type& public_key )
          {
            if( public_key == _public_from )
              return fc::optional<fc::crypto_data::private_key_type>( _private_from );
            return fc::optional<fc::crypto_data::private_key_type>();
          };

          auto _private_key_finder_to = [&_public_to, &_private_to]( const fc::crypto_data::public_key_type& public_key )
          {
            if( public_key == _public_to )
              return fc::optional<fc::crypto_data::private_key_type>( _private_to );
            return fc::optional<fc::crypto_data::private_key_type>();
          };

          auto _result_00 = _cd.decrypt( _private_key_finder_from, _public_from, _public_to, _encrypted_content );
          BOOST_REQUIRE_EQUAL( _result_00, content );

          auto _result_01 = _cd.decrypt( _private_key_finder_to, _public_from, _public_to, _encrypted_content );
          BOOST_REQUIRE_EQUAL( _result_01, content );
        }
      };

      auto _the_same_keys = []( const std::string& content )
      {
        //Using the `crypto_data` class do encryption/decryption for many `from`, `to` keys.
        fc::crypto_data _cd;

        for( auto i = 0; i < 1000; ++i )
        {
          auto _private = fc::ecc::private_key::generate();
          auto _public  = _private.get_public_key();

          std::string _encrypted_content = _cd.encrypt( _private, _public, content );

          auto _private_key_finder = [&_public, &_private]( const fc::crypto_data::public_key_type& public_key )
          {
            if( public_key == _public )
              return fc::optional<fc::crypto_data::private_key_type>( _private );
            return fc::optional<fc::crypto_data::private_key_type>();
          };

          auto _result = _cd.decrypt( _private_key_finder, _public, _public, _encrypted_content );
          BOOST_REQUIRE_EQUAL( _result, content );
        }
      };

      _different_keys( _fruits_content );
      _different_keys( _hex_content );
      _different_keys( _empty_content );

      _the_same_keys( _fruits_content );
      _the_same_keys( _hex_content );
      _the_same_keys( _empty_content );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(import_keys)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto _prefix = "STM";

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    const std::string _wallet_name = "wallet-0";

    bk.create_wallet( _token, _wallet_name, "", false );

    // core_minimal saves to disk after each key (no batch mode), so use fewer keys
    const size_t _nr_keys = 1'000;
    std::vector<std::string> _keys( _nr_keys );

    for( size_t i = 0; i < _nr_keys; ++i )
    {
      auto _priv = fc::ecc::private_key::generate();
      _keys[i] = _priv.key_to_wif();
    }

    auto _start = std::chrono::high_resolution_clock::now();

    for( const auto& wif : _keys )
      bk.import_key( _wallet_name, wif, _prefix );

    auto _duration = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now() - _start );
    auto  _time = _duration.count();

    BOOST_REQUIRE_LT( _time, 10000 );

    BOOST_TEST_MESSAGE( std::to_string( _time ) + " [ms]" );

    auto _public_keys = bk.get_public_keys( _wallet_name );
    BOOST_REQUIRE_EQUAL( _public_keys.size(), _nr_keys );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(has_wallet)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    const std::string _wallet_name = "wallet";
    const std::string _wallet_name_2 = "wallet-2";
    const std::string _wallet_name_3 = "wallet-3";
    const std::string _password = "avocado";

    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name_3 ), false );
    bk.create_wallet( _token, _wallet_name_3, _password, true );
    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name_3 ), true );

    bk.close_wallet( _wallet_name_3 );
    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name_3 ), false ); // temporary — no file on disk

    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name ), false );
    bk.create_wallet( _token, _wallet_name, _password, false );
    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name ), true );

    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name_2 ), false );
    bk.create_wallet( _token, _wallet_name_2, _password, false );
    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name ), true );

    bk.close_wallet( _wallet_name );
    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name ), true ); // file exists

    bk.close_wallet( _wallet_name_2 );
    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name_2 ), true ); // file exists

    bk.open_wallet( _token, _wallet_name );
    bk.unlock( _wallet_name, _password );
    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name ), true );

    bk.lock( _wallet_name );
    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name ), true );
    BOOST_REQUIRE_EQUAL( bk.has_wallet( _wallet_name_2 ), true );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(temporary_wallets)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto _prefix = "STM";

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    struct key_info
    {
      std::string private_key;
      std::string public_key;
    };
    std::vector<key_info> _keys =
    {
      {"5J15npVK6qABGsbdsLnJdaF5esrEWxeejeE3KUx6r534ug4tyze", "STM6TqSJaS1aRj6p6yZEo5xicX7bvLhrfdVqi5ToNrKxHU3FRBEdW"},
      {"5K1gv5rEtHiACVTFq9ikhEijezMh4rkbbTPqu4CAGMnXcTLC1su", "STM8LbCRyqtXk5VKbdFwK1YBgiafqprAd7yysN49PnDwAsyoMqQME"},
      {"5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw", "STM8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4"},
      {"5KXNQP5feaaXpp28yRrGaFeNYZT7Vrb1PqLEyo7E3pJiG1veLKG", "STM6a34GANY5LD8deYvvfySSWGd7sPahgVNYoFPapngMUD27pWb45"}
    };

    const std::string _wallet_name_0 = "wallet-0";
    const std::string _wallet_name_1 = "wallet-1";
    const std::string _wallet_name_2 = "wallet-2";
    const std::string _password = "avocado";

    bk.create_wallet( _token, _wallet_name_0, _password, false );
    {
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_0 ) == true );
    }

    bk.create_wallet( _token, _wallet_name_1, _password, true/*is_temporary*/  );
    {
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_0 ) == true );
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_1 ) == false );
    }

    bk.create_wallet( _token, _wallet_name_2, _password, true/*is_temporary*/  );
    {
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_0 ) == true );
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_1 ) == false );
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_2 ) == false );
    }

    bk.import_key( _wallet_name_1, _keys[0].private_key, _prefix );
    {
      auto _public_keys = bk.get_public_keys( _wallet_name_1 );
      BOOST_REQUIRE( _public_keys.size() == 1 );
      auto min_pub_0 = b_mgr.crypto.public_key_from_string( _keys[0].public_key, _prefix );
      BOOST_REQUIRE( _public_keys.find( min_pub_0 ) != _public_keys.end() );
    }

    bk.import_key( _wallet_name_2, _keys[1].private_key, _prefix );
    bk.import_key( _wallet_name_2, _keys[2].private_key, _prefix );
    {
      auto _public_keys = bk.get_public_keys( _wallet_name_2 );
      BOOST_REQUIRE( _public_keys.size() == 2 );
      auto min_pub_1 = b_mgr.crypto.public_key_from_string( _keys[1].public_key, _prefix );
      auto min_pub_2 = b_mgr.crypto.public_key_from_string( _keys[2].public_key, _prefix );
      BOOST_REQUIRE( _public_keys.find( min_pub_1 ) != _public_keys.end() );
      BOOST_REQUIRE( _public_keys.find( min_pub_2 ) != _public_keys.end() );
    }

    auto min_pub_1 = b_mgr.crypto.public_key_from_string( _keys[1].public_key, _prefix );
    bk.remove_key( _wallet_name_2, min_pub_1 );
    {
      auto _public_keys = bk.get_public_keys( _wallet_name_2 );
      BOOST_REQUIRE( _public_keys.size() == 1 );
      auto min_pub_2 = b_mgr.crypto.public_key_from_string( _keys[2].public_key, _prefix );
      BOOST_REQUIRE( _public_keys.find( min_pub_2 ) != _public_keys.end() );
    }

    bk.close_wallet( _wallet_name_2 );
    {
      // Temporary wallet "wallet-2" is gone after close (no file)
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_0 ) == true );
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_1 ) == false ); // temporary
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_2 ) == false ); // temporary, closed
    }

    bk.close_wallet( _wallet_name_0 );
    {
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_0 ) == true ); // persistent file
      BOOST_REQUIRE( b_mgr.exists_wallet( _wallet_name_1 ) == false ); // temporary
    }

    bk.close_wallet( _wallet_name_1 );
    {
      // Only persistent wallet remains on disk
      auto files = b_mgr.storage->list_dir();
      BOOST_REQUIRE( files.size() == 1 );
      BOOST_REQUIRE( files[0] == _wallet_name_0 );
    }

    // Can't open a temporary wallet that was closed
    BOOST_REQUIRE_THROW( bk.open_wallet( _token, _wallet_name_1 ), std::exception );

    // Can reopen persistent wallet
    bk.open_wallet( _token, _wallet_name_0 );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(wallets_synchronization)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto _prefix = "STM";

    auto bk = b_mgr.create_beekeeper();

    auto _token_00 = bk.create_session();
    auto _token_01 = bk.create_session();

    const std::string _wallet_name = "wallet-0";

    bk.create_wallet( _token_00, _wallet_name, "avocado", false );
    // Second session opens the same wallet (already unlocked from create)
    bk.open_wallet( _token_01, _wallet_name );

    std::vector<std::pair<std::string, std::string>> _keys =
      {
        { "5KGKYWMXReJewfj5M29APNMqGEu173DzvHv5TeJAg9SkjUeQV78", "oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa" },
        { "5KLytoW1AiGSoHHBA73x1AmgZnN16QDgU1SPpG9Vd2dpdiBgSYw", "8FDsHdPkHbY8fuUkVLyAmrnKMvj6DddLopi3YJ51dVqsG9vZa4" },
        { "5KKvoNaCPtN9vUEU1Zq9epSAVsEPEtocbJsp7pjZndt9Rn4dNRg", "8mmxXz5BfQc2NJfqhiPkbgcyJm4EvWEr2UAUdr56gEWSN9ZnA5" }
      };

    bk.import_key( _wallet_name, _keys[0].first, _prefix );
    {
      auto _public_keys = bk.get_public_keys( _wallet_name );
      BOOST_REQUIRE_EQUAL( _public_keys.size(), 1 );
    }
    bk.import_key( _wallet_name, _keys[1].first, _prefix );
    bk.import_key( _wallet_name, _keys[2].first, _prefix );
    {
      auto _public_keys = bk.get_public_keys( _wallet_name );
      BOOST_REQUIRE_EQUAL( _public_keys.size(), 3 );
    }

    bk.close_wallet( _wallet_name );
    bk.open_wallet( _token_00, _wallet_name );
    bk.unlock( _wallet_name, "avocado" );

    auto _public_keys = bk.get_public_keys( _wallet_name );
    BOOST_REQUIRE_EQUAL( _public_keys.size(), _keys.size() );

  } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(is_wallet_unlocked)
{
  try
  {
    test_utils::beekeeper_mgr b_mgr;
    b_mgr.remove_wallets();

    auto bk = b_mgr.create_beekeeper();
    auto _token = bk.create_session();

    const std::string _wallet_name = "wallet_name";
    const std::string _password = "avocado";

    // Wallet doesn't exist yet — has_wallet returns false
    BOOST_REQUIRE( bk.has_wallet( _wallet_name ) == false );

    bk.create_wallet( _token, _wallet_name, _password, false );

    // After creation, wallet is unlocked
    {
      auto wallets = bk.list_wallets(_token);
      for (auto& w : wallets)
        if (w.name == _wallet_name)
          BOOST_REQUIRE(w.unlocked);
    }

    bk.lock( _wallet_name );

    // After lock, wallet is locked
    {
      auto wallets = bk.list_wallets(_token);
      for (auto& w : wallets)
        if (w.name == _wallet_name)
          BOOST_REQUIRE(!w.unlocked);
    }

    bk.unlock( _wallet_name, _password );

    // After unlock, wallet is unlocked
    {
      auto wallets = bk.list_wallets(_token);
      for (auto& w : wallets)
        if (w.name == _wallet_name)
          BOOST_REQUIRE(w.unlocked);
    }

    bk.close_wallet( _wallet_name );

    // After close, wallet exists on disk but not in memory
    BOOST_REQUIRE( bk.has_wallet( _wallet_name ) == true );

  } FC_LOG_AND_RETHROW()
}

// ── Wallet-at-rest KDF format (v1) ─────────────────────────────

namespace {

constexpr char v1_magic[] = "BEEKWLT"; // 7 bytes, see crypto_provider_impl.cpp
constexpr size_t v1_iterations_offset = 9;  // magic[7] + version + kdf_id
constexpr size_t v1_salt_offset       = 14; // ... + iterations u32 + salt_len

bool blob_has_v1_magic(const std::vector<char>& blob)
{
  return blob.size() >= 7 && std::memcmp(blob.data(), v1_magic, 7) == 0;
}

uint32_t blob_iterations(const std::vector<char>& blob)
{
  uint32_t iterations = 0;
  std::memcpy(&iterations, blob.data() + v1_iterations_offset, 4);
  return iterations;
}

/// Reproduces the pre-v1 wallet scheme byte-for-byte: AES-256-CBC keyed by one
/// unsalted SHA-512 of the password, plaintext = sha512(password) || keys.
std::vector<char> make_legacy_wallet_blob(const std::string& password,
                                          const beekeeper_minimal::keys_map& keys)
{
  beekeeper_minimal::fc_crypto_primitives prims;
  auto pw = prims.sha512(reinterpret_cast<const uint8_t*>(password.data()), password.size());
  auto plain = beekeeper_minimal::pack_plain_keys(pw, keys);
  auto ct = prims.aes256_cbc_encrypt(pw.data.data(), pw.data.data() + 32,
                                     reinterpret_cast<const uint8_t*>(plain.data()), plain.size());
  return std::vector<char>(reinterpret_cast<const char*>(ct.data()),
                           reinterpret_cast<const char*>(ct.data()) + ct.size());
}

} // anonymous namespace

BOOST_AUTO_TEST_CASE(wallet_kdf_v1_format)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  bk.create_wallet(_token, "v1", "pass", false);
  auto priv = fc::ecc::private_key::generate();
  auto wif = priv.key_to_wif();
  bk.import_key("v1", wif, "STM");

  // Stored blob is the self-describing v1 format with the provider's work factor.
  auto blob = b_mgr.read_wallet_blob("v1");
  BOOST_REQUIRE(blob_has_v1_magic(blob));
  BOOST_REQUIRE_EQUAL(1, static_cast<int>(blob[7])); // format version
  BOOST_REQUIRE_EQUAL(1, static_cast<int>(blob[8])); // kdf id: PBKDF2-HMAC-SHA512
  BOOST_REQUIRE_EQUAL(test_utils::beekeeper_mgr::test_kdf_iterations, blob_iterations(blob));

  // Wallet files are written owner-only.
  auto perms = boost::filesystem::status(b_mgr.dir / "v1.wallet").permissions();
  BOOST_REQUIRE_EQUAL(static_cast<int>(perms & boost::filesystem::all_all),
                      static_cast<int>(boost::filesystem::owner_read | boost::filesystem::owner_write));

  // Full persistence roundtrip.
  bk.close_wallet("v1");
  bk.open_wallet(_token, "v1");
  BOOST_REQUIRE_THROW(bk.unlock("v1", "wrong"), std::exception);
  bk.unlock("v1", "pass");
  auto keys = bk.get_public_keys("v1");
  BOOST_REQUIRE_EQUAL(1u, keys.size());
  BOOST_REQUIRE_EQUAL(wif, b_mgr.crypto.key_to_wif(keys.begin()->second.first));

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(wallet_legacy_migration)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  // Plant a wallet file written exactly as the legacy scheme wrote it.
  auto wif = fc::ecc::private_key::generate().key_to_wif();
  auto priv = b_mgr.crypto.wif_to_key(wif);
  BOOST_REQUIRE(priv.has_value());
  auto pub = b_mgr.crypto.get_public_key(*priv);
  beekeeper_minimal::keys_map keys;
  keys.emplace(pub, beekeeper_minimal::key_data(*priv, "STM"));

  auto legacy_blob = make_legacy_wallet_blob("legacy-pass", keys);
  BOOST_REQUIRE(!blob_has_v1_magic(legacy_blob));
  b_mgr.write_wallet_blob("legacy", legacy_blob);

  // Wrong password on the legacy path is rejected and must not migrate.
  bk.open_wallet(_token, "legacy");
  BOOST_REQUIRE_THROW(bk.unlock("legacy", "wrong-legacy-pass"), std::exception);
  BOOST_REQUIRE(!blob_has_v1_magic(b_mgr.read_wallet_blob("legacy")));

  // The legacy wallet unlocks with the correct password...
  bk.unlock("legacy", "legacy-pass");
  auto loaded = bk.get_public_keys("legacy");
  BOOST_REQUIRE_EQUAL(1u, loaded.size());
  BOOST_REQUIRE_EQUAL(wif, b_mgr.crypto.key_to_wif(loaded.begin()->second.first));

  // ...and the file was transparently re-encrypted with the v1 format.
  auto migrated = b_mgr.read_wallet_blob("legacy");
  BOOST_REQUIRE(blob_has_v1_magic(migrated));

  // The migrated wallet behaves like any other: wrong password rejected,
  // correct password recovers the same key.
  bk.close_wallet("legacy");
  bk.open_wallet(_token, "legacy");
  BOOST_REQUIRE_THROW(bk.unlock("legacy", "bad"), std::exception);
  bk.unlock("legacy", "legacy-pass");
  loaded = bk.get_public_keys("legacy");
  BOOST_REQUIRE_EQUAL(wif, b_mgr.crypto.key_to_wif(loaded.begin()->second.first));

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(wallet_v1_tamper_detection)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  bk.create_wallet(_token, "tamper", "pass", false);
  bk.import_key("tamper", fc::ecc::private_key::generate().key_to_wif(), "STM");
  auto good = b_mgr.read_wallet_blob("tamper");
  bk.close_wallet("tamper");

  // Flipping a single bit anywhere — salt, ciphertext or tag — must be rejected.
  const size_t probes[] = { v1_salt_offset + 3,  // inside salt
                            good.size() - 40,    // inside ciphertext
                            good.size() - 1 };   // inside HMAC tag
  for (auto pos : probes)
  {
    auto bad = good;
    bad[pos] = static_cast<char>(bad[pos] ^ 0x01);
    b_mgr.write_wallet_blob("tamper", bad);
    bk.open_wallet(_token, "tamper");
    BOOST_REQUIRE_THROW(bk.unlock("tamper", "pass"), std::exception);
    bk.close_wallet("tamper");
  }

  // The untampered blob still unlocks.
  b_mgr.write_wallet_blob("tamper", good);
  bk.open_wallet(_token, "tamper");
  bk.unlock("tamper", "pass");

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(wallet_v1_salt_uniqueness)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  // Identical password and identical (empty) key set must never produce
  // identical files — salt and IV are drawn fresh for every encryption.
  bk.create_wallet(_token, "salt_a", "same-pass", false);
  bk.create_wallet(_token, "salt_b", "same-pass", false);
  auto blob_a = b_mgr.read_wallet_blob("salt_a");
  auto blob_b = b_mgr.read_wallet_blob("salt_b");
  BOOST_REQUIRE(blob_a != blob_b);

  // Re-encrypting the same wallet refreshes the salt as well.
  bk.import_key("salt_a", fc::ecc::private_key::generate().key_to_wif(), "STM");
  auto blob_a2 = b_mgr.read_wallet_blob("salt_a");
  BOOST_REQUIRE(!std::equal(blob_a.begin() + v1_salt_offset, blob_a.begin() + v1_salt_offset + 16,
                            blob_a2.begin() + v1_salt_offset));

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(wallet_v1_kdf_iteration_bounds)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  bk.create_wallet(_token, "bounds", "pass", false);
  auto good = b_mgr.read_wallet_blob("bounds");
  bk.close_wallet("bounds");

  // Iteration counts of zero or beyond the sanity cap are rejected while the
  // header is parsed — before any KDF work is attempted, so a hostile file
  // cannot turn unlock into a multi-minute stall. The message assertion pins
  // the parse-time rejection: a KDF/MAC failure would say "Invalid password".
  auto is_iteration_range_error = [](const std::exception& e) {
    return std::string(e.what()).find("iteration count out of range") != std::string::npos;
  };
  for (uint32_t bad_iterations : { 0u, 10'000'001u, 0xFFFFFFFFu })
  {
    auto bad = good;
    std::memcpy(bad.data() + v1_iterations_offset, &bad_iterations, 4);
    b_mgr.write_wallet_blob("bounds", bad);
    bk.open_wallet(_token, "bounds");
    BOOST_REQUIRE_EXCEPTION(bk.unlock("bounds", "pass"), std::exception, is_iteration_range_error);
    bk.close_wallet("bounds");
  }

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(wallet_v1_header_rejections)
{ try {
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  bk.create_wallet(_token, "hdr", "pass", false);
  auto good = b_mgr.read_wallet_blob("hdr");
  bk.close_wallet("hdr");

  auto expect_rejection = [&](std::vector<char> bad, const std::string& message_part)
  {
    b_mgr.write_wallet_blob("hdr", bad);
    bk.open_wallet(_token, "hdr");
    BOOST_REQUIRE_EXCEPTION(bk.unlock("hdr", "pass"), std::exception,
      [&](const std::exception& e) {
        return std::string(e.what()).find(message_part) != std::string::npos;
      });
    bk.close_wallet("hdr");
  };

  // Every unknown/invalid header field is rejected with a distinct error,
  // before any KDF work.
  { auto bad = good; bad[7]  = 2;   expect_rejection(bad, "format version"); }
  { auto bad = good; bad[8]  = 2;   expect_rejection(bad, "Unsupported wallet KDF"); }
  { auto bad = good; bad[13] = 7;   expect_rejection(bad, "salt length out of range"); }
  { auto bad = good; bad[13] = 65;  expect_rejection(bad, "salt length out of range"); }
  { auto bad = good; bad[30] = 15;  expect_rejection(bad, "IV length invalid"); }
  { auto bad = good; bad.resize(60);            expect_rejection(bad, "truncated"); }
  { auto bad = good; bad.resize(47 + 32 + 17);  expect_rejection(bad, "ciphertext length invalid"); }

  // The pristine blob still unlocks after all those rejections.
  b_mgr.write_wallet_blob("hdr", good);
  bk.open_wallet(_token, "hdr");
  bk.unlock("hdr", "pass");

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(wallet_legacy_empty_password)
{ try {
  // Historical beekeeper allowed creating wallets with an explicitly empty
  // password. Such wallets must keep unlocking, migrate cleanly, and stay
  // writable afterwards.
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  auto wif = fc::ecc::private_key::generate().key_to_wif();
  auto priv = b_mgr.crypto.wif_to_key(wif);
  BOOST_REQUIRE(priv.has_value());
  beekeeper_minimal::keys_map keys;
  keys.emplace(b_mgr.crypto.get_public_key(*priv), beekeeper_minimal::key_data(*priv, "STM"));

  b_mgr.write_wallet_blob("legacy_empty", make_legacy_wallet_blob("", keys));

  bk.open_wallet(_token, "legacy_empty");
  bk.unlock("legacy_empty", "");
  BOOST_REQUIRE_EQUAL(1u, bk.get_public_keys("legacy_empty").size());
  BOOST_REQUIRE(blob_has_v1_magic(b_mgr.read_wallet_blob("legacy_empty")));

  // State-changing operations (each re-encrypts) still work...
  bk.import_key("legacy_empty", fc::ecc::private_key::generate().key_to_wif(), "STM");
  BOOST_REQUIRE_EQUAL(2u, bk.get_public_keys("legacy_empty").size());

  // ...and the migrated v1 wallet unlocks with the empty password again.
  bk.close_wallet("legacy_empty");
  bk.open_wallet(_token, "legacy_empty");
  BOOST_REQUIRE_THROW(bk.unlock("legacy_empty", "not-empty"), std::exception);
  bk.unlock("legacy_empty", "");
  BOOST_REQUIRE_EQUAL(2u, bk.get_public_keys("legacy_empty").size());

} FC_LOG_AND_RETHROW() }

namespace {

/// Storage wrapper whose save() can be switched to fail — simulates a
/// read-only wallet dir without depending on filesystem permissions
/// (which do not stop root, e.g. in CI containers).
struct failing_save_storage final : public beekeeper_minimal::wallet_storage
{
  beekeeper_minimal::wallet_storage& inner;
  bool fail_save = true;

  explicit failing_save_storage(beekeeper_minimal::wallet_storage& s) : inner(s) {}

  void save(const std::string& path, const std::vector<char>& buffer) override
  {
    if (fail_save)
      throw std::runtime_error("storage unavailable");
    inner.save(path, buffer);
  }
  std::vector<char> load(const std::string& path) override { return inner.load(path); }
  bool scan_dir(const std::string& wallet_name) override { return inner.scan_dir(wallet_name); }
};

} // anonymous namespace

BOOST_AUTO_TEST_CASE(wallet_legacy_migration_storage_failure)
{ try {
  // Migration is opportunistic: when storage cannot be written, unlocking a
  // legacy wallet must still succeed, leave the file untouched, and retry the
  // migration on a later unlock once storage works again.
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto wif = fc::ecc::private_key::generate().key_to_wif();
  auto priv = b_mgr.crypto.wif_to_key(wif);
  BOOST_REQUIRE(priv.has_value());
  beekeeper_minimal::keys_map keys;
  keys.emplace(b_mgr.crypto.get_public_key(*priv), beekeeper_minimal::key_data(*priv, "STM"));

  b_mgr.write_wallet_blob("legacy_ro", make_legacy_wallet_blob("pass", keys));

  failing_save_storage storage(*b_mgr.storage);
  beekeeper_minimal::wallet w(b_mgr.crypto, &storage, "legacy_ro");
  w.open();
  w.unlock("pass");
  BOOST_REQUIRE(!w.is_locked());
  BOOST_REQUIRE_EQUAL(1u, w.get_keys().size());
  BOOST_REQUIRE(!blob_has_v1_magic(b_mgr.read_wallet_blob("legacy_ro"))); // untouched

  // Storage recovers: the next unlock performs the migration.
  storage.fail_save = false;
  w.lock();
  w.unlock("pass");
  BOOST_REQUIRE_EQUAL(1u, w.get_keys().size());
  BOOST_REQUIRE(blob_has_v1_magic(b_mgr.read_wallet_blob("legacy_ro")));

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(wallet_v1_external_fixture)
{ try {
  // Fixtures generated by an independent reference implementation (python
  // hashlib/hmac + openssl CLI); shared with the WASM test suite
  // (__tests__/detailed/wallet_format.ts). Unlocking them proves the C++
  // implementation interoperates with the documented format, not just with
  // itself. Password "fixture-pass", one imported key.
  const std::string fixture_password = "fixture-pass";
  const std::string fixture_pub = "STM5RqVBAVNp5ufMCetQtvLGLJo7unX9nyCBMMrTXRWQ9i1Zzzizh";
  const std::string legacy_hex =
    "23279e80bad106e06b60ee11322d79f92e2dd72eed60bcba837b05f90e50128ef0cf23d2ed81afcbee559a5c2acccd06"
    "43e5a21d60ba61c7b2f6ed78a29e98403085382f536bf197c60af92641e71662eaf531e166ce07ba3fb1143d3bed942e"
    "daaced3e1e6b30e5d5f33521b67cc58874cb617f6d9c7dd56caad67a10ef8940c500d816facf721cb8e610dd986d277e";
  const std::string v1_hex =
    "4245454b574c5401010010000010000102030405060708090a0b0c0d0e0f10101112131415161718191a1b1c1d1e1f"
    "67edcb46992feedbfab1e8849069a75d14e89bca7c154384de797b7b49c51e078100080657f8b5dbe2052911dae498"
    "db1845bb40f9d9dc4ce80a21756258fc716f2dc2566863028f82b442b43d73301fca2753d806abc28fbb1cad32c167"
    "ebc467b0696442b2ca669ac534bc17311856";
  // Same wallet, but with a 32-byte salt — exercises variable-salt-length parsing.
  const std::string v1_salt32_hex =
    "4245454b574c5401010010000020202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f10"
    "404142434445464748494a4b4c4d4e4f216d7ca85bdf59bf8353a3cf6d5602f113a9b5a5e08a2ebecabc3c8430035b"
    "99bcb9faef160dccc3b55bbbb55807029ad9d7af21841406d1ee542626109537c4c5651c7a7edf98184a4004406cf9"
    "a364450de7dfbc2405a646065dafcf0dff690fc0d4710e033ddfd6cc50bd03e45882";

  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  auto bk = b_mgr.create_beekeeper();
  auto _token = bk.create_session();

  for (const auto& [name, hex] : { std::pair<std::string, std::string>{"fixture_legacy", legacy_hex},
                                   std::pair<std::string, std::string>{"fixture_v1", v1_hex},
                                   std::pair<std::string, std::string>{"fixture_v1_salt32", v1_salt32_hex} })
  {
    b_mgr.write_wallet_blob(name, beekeeper_minimal::hex_decode(hex));
    bk.open_wallet(_token, name);
    bk.unlock(name, fixture_password);
    auto keys = bk.get_public_keys(name);
    BOOST_REQUIRE_EQUAL(1u, keys.size());
    BOOST_REQUIRE_EQUAL(fixture_pub,
      b_mgr.crypto.public_key_to_string(keys.begin()->first, keys.begin()->second.second));
  }

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE(wallet_default_kdf_profile)
{ try {
  // One end-to-end pass with the production work factor, so the default
  // parameters stay exercised even though the rest of the suite lowers them.
  test_utils::beekeeper_mgr b_mgr;
  b_mgr.remove_wallets();

  beekeeper_minimal::fc_crypto_provider default_crypto;
  beekeeper_minimal::beekeeper bk(default_crypto, *b_mgr.storage, 900);
  auto _token = bk.create_session();

  bk.create_wallet(_token, "prod", "pass", false);
  auto blob = b_mgr.read_wallet_blob("prod");
  BOOST_REQUIRE(blob_has_v1_magic(blob));
  BOOST_REQUIRE_EQUAL(beekeeper_minimal::crypto_provider_impl::default_kdf_iterations,
                      blob_iterations(blob));
  bk.lock("prod");
  bk.unlock("prod", "pass");

  // Wallets are self-describing: one written with a different work factor
  // (the fast test profile) unlocks fine under the default provider.
  {
    auto bk_fast = b_mgr.create_beekeeper();
    auto _token_fast = bk_fast.create_session();
    bk_fast.create_wallet(_token_fast, "fast", "pass2", false);
  }
  bk.open_wallet(_token, "fast");
  bk.unlock("fast", "pass2");

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
#endif
