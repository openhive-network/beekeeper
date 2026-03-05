#include <beekeeper/beekeeper_wallet_api.hpp>
#include <beekeeper/extended_api.hpp>

#include <hive/protocol/config.hpp>

#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <fc/variant_object.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/git_revision.hpp>

namespace beekeeper {

#define HIVE_BEEKEEPER_API_NAME "beekeeper_api"

namespace detail {

class beekeeper_api_impl
{
  private:

    const std::string prefix;

    extended_api ex_api;

    beekeeper_minimal::public_key_type pub_from_string( const std::string& source )
    {
      return _crypto.public_key_from_string( source, prefix );
    }

  public:
    beekeeper_api_impl( beekeeper_minimal::beekeeper& bk, beekeeper_minimal::crypto_provider& crypto,
                        beekeeper_minimal::wallet_storage& storage,
                        std::shared_ptr<mutex_handler> mtx_handler, appbase::application& app, uint64_t unlock_interval )
                      : prefix( HIVE_ADDRESS_PREFIX ),
                        ex_api( unlock_interval ), _bk( bk ), _crypto( crypto ), _storage( storage ), _mtx_handler( mtx_handler )
                        {
                          FC_ASSERT( _mtx_handler );
                        }

    DECLARE_API_IMPL
    (
      (create)
      (open)
      (close)
      (set_timeout)
      (lock_all)
      (lock)
      (unlock)
      (import_key)
      (import_keys)
      (remove_key)
      (list_wallets)
      (list_created_wallets)
      (get_public_keys)
      (sign_digest)
      (get_info)
      (create_session)
      (close_session)
      (has_matching_private_key)
      (encrypt_data)
      (decrypt_data)
      (get_version)
      (has_wallet)
      (is_wallet_unlocked)
    )

    beekeeper_minimal::beekeeper& _bk;
    beekeeper_minimal::crypto_provider& _crypto;
    beekeeper_minimal::wallet_storage& _storage;
    std::shared_ptr<mutex_handler> _mtx_handler;
};

DEFINE_API_IMPL( beekeeper_api_impl, create )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  auto pw = _bk.create_wallet( args.token, args.wallet_name, args.password.value_or(""), args.is_temporary );
  return { pw };
}

DEFINE_API_IMPL( beekeeper_api_impl, open )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  _bk.open_wallet( args.token, args.wallet_name );
  return open_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, close )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  _bk.close_wallet( args.wallet_name );
  return close_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, set_timeout )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  _bk.set_timeout( args.seconds );
  return set_timeout_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, lock_all )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  _bk.lock_all();
  return lock_all_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, lock )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  _bk.lock( args.wallet_name );
  return lock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, unlock )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );

  if( ex_api.unlock_allowed() )
  {
    try
    {
      _bk.unlock( args.wallet_name, args.password );
    }
    FC_CAPTURE_CALL_LOG_AND_RETHROW(([this]()
      {
        ex_api.was_error();
      }), ());
  }
  else
    FC_ASSERT(false, "unlock is not accessible");

  return unlock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, import_key )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  auto pub_str = _bk.import_key( args.wallet_name, args.wif_key, prefix );
  return { pub_str };
}

DEFINE_API_IMPL( beekeeper_api_impl, import_keys )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  auto pub_strs = _bk.import_keys( args.wallet_name, args.wif_keys, prefix );
  return { pub_strs };
}

DEFINE_API_IMPL( beekeeper_api_impl, remove_key )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  auto pub = pub_from_string( args.public_key );
  _bk.remove_key( args.wallet_name, pub );
  return remove_key_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, list_wallets )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  auto wallets = _bk.list_wallets( args.token );
  flat_set<wallet_details> result;
  for( auto& w : wallets )
    result.insert( wallet_details{ w.name, w.unlocked } );
  return { result };
}

DEFINE_API_IMPL( beekeeper_api_impl, list_created_wallets )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  auto names = _storage.list_dir();
  flat_set<wallet_details> result;
  for( auto& name : names )
    result.insert( wallet_details{ name, false } );
  return { result };
}

DEFINE_API_IMPL( beekeeper_api_impl, get_public_keys )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );

  flat_set<public_key_details> result;

  if( args.wallet_name.has_value() )
  {
    auto keys = _bk.get_public_keys( *args.wallet_name );
    for( auto& [pub, kd] : keys )
      result.insert( public_key_details{ _crypto.public_key_to_string( pub, kd.second ) } );
  }
  else
  {
    // Get keys from all unlocked wallets in this session
    auto wallets = _bk.list_wallets( args.token );
    for( auto& w : wallets )
    {
      if( w.unlocked )
      {
        auto keys = _bk.get_public_keys( w.name );
        for( auto& [pub, kd] : keys )
          result.insert( public_key_details{ _crypto.public_key_to_string( pub, kd.second ) } );
      }
    }
  }

  return { result };
}

DEFINE_API_IMPL( beekeeper_api_impl, sign_digest )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );

  auto digest = _crypto.digest_from_hex( args.sig_digest );
  auto pub = pub_from_string( args.public_key );

  if( args.wallet_name.has_value() )
  {
    auto sig = _bk.sign_digest( *args.wallet_name, digest, pub, prefix );
    return { _crypto.signature_to_hex( sig ) };
  }
  else
  {
    // Search all unlocked wallets in this session
    auto wallets = _bk.list_wallets( args.token );
    for( auto& w : wallets )
    {
      if( w.unlocked )
      {
        if( _bk.has_private_key( w.name, pub ) )
        {
          auto sig = _bk.sign_digest( w.name, digest, pub, prefix );
          return { _crypto.signature_to_hex( sig ) };
        }
      }
    }

    FC_ASSERT( false, "Public key ${key} not found in any unlocked wallet", ("key", args.public_key) );
  }
}

DEFINE_API_IMPL( beekeeper_api_impl, get_info )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  return _bk.get_info();
}

DEFINE_API_IMPL( beekeeper_api_impl, create_session )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  auto token = _bk.create_session();
  return { token };
}

DEFINE_API_IMPL( beekeeper_api_impl, close_session )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.close_session( args.token );
  return close_session_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, has_matching_private_key )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  auto pub = pub_from_string( args.public_key );
  return { _bk.has_private_key( args.wallet_name, pub ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, encrypt_data )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  auto from = pub_from_string( args.from_public_key );
  auto to = pub_from_string( args.to_public_key );
  uint64_t nonce = args.nonce.value_or( 0 );
  auto result = _bk.encrypt_data( args.wallet_name, from, to, args.content, prefix, nonce );
  return { result };
}

DEFINE_API_IMPL( beekeeper_api_impl, decrypt_data )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  auto from = pub_from_string( args.from_public_key );
  auto to = pub_from_string( args.to_public_key );
  auto result = _bk.decrypt_data( args.wallet_name, from, to, args.encrypted_content, prefix );
  return { result };
}

DEFINE_API_IMPL( beekeeper_api_impl, get_version )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  return { fc::git_revision_sha };
}

DEFINE_API_IMPL( beekeeper_api_impl, has_wallet )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  return { _bk.has_wallet( args.wallet_name ) };
}

DEFINE_API_IMPL( beekeeper_api_impl, is_wallet_unlocked )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  auto wallets = _bk.list_wallets( args.token );
  for( auto& w : wallets )
  {
    if( w.name == args.wallet_name )
      return { w.unlocked };
  }
  return { false };
}

} // detail

beekeeper_wallet_api::beekeeper_wallet_api( beekeeper_minimal::beekeeper& bk, beekeeper_minimal::crypto_provider& crypto,
                                            beekeeper_minimal::wallet_storage& storage,
                                            std::shared_ptr<mutex_handler> mtx_handler,
                                            appbase::application& app, uint64_t unlock_interval )
                    : my( new detail::beekeeper_api_impl( bk, crypto, storage, mtx_handler, app, unlock_interval ) )
{
  JSON_RPC_REGISTER_API( HIVE_BEEKEEPER_API_NAME );
}

beekeeper_wallet_api::~beekeeper_wallet_api() {}

DEFINE_LOCKLESS_APIS( beekeeper_wallet_api,
  (create)
  (open)
  (close)
  (set_timeout)
  (lock_all)
  (lock)
  (unlock)
  (import_key)
  (import_keys)
  (remove_key)
  (list_wallets)
  (list_created_wallets)
  (get_public_keys)
  (sign_digest)
  (get_info)
  (create_session)
  (close_session)
  (has_matching_private_key)
  (encrypt_data)
  (decrypt_data)
  (get_version)
  (has_wallet)
  (is_wallet_unlocked)
  )

} // beekeeper
