#include <beekeeper/beekeeper_wallet_api.hpp>
#include <beekeeper/extended_api.hpp>
#include <beekeeper/file_storage.hpp>

#include <hive/protocol/config.hpp>

#include <hive/plugins/json_rpc/json_rpc_plugin.hpp>

#include <fc/variant_object.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/git_revision.hpp>
#include <fc/exception/exception.hpp>

#include <cctype>
#include <optional>

namespace beekeeper {

#define HIVE_BEEKEEPER_API_NAME "beekeeper_api"

namespace detail {

class beekeeper_api_impl
{
  private:

    const std::string prefix;

    extended_api ex_api;

    static bool is_wallet_name_valid( const std::string& name )
    {
      if( name.empty() )
        return false;

      for( char ch : name )
      {
        if( std::isalnum( static_cast<unsigned char>( ch ) ) )
          continue;
        if( ch == '.' || ch == '_' || ch == '-' || ch == '@' )
          continue;
        return false;
      }
      return true;
    }

    std::optional<beekeeper_minimal::wallet_details> find_session_wallet( const std::string& token, const std::string& name )
    {
      auto wallets = _bk.list_wallets( token );
      for( const auto& w : wallets )
      {
        if( w.name == name )
          return w;
      }
      return std::nullopt;
    }

    void throw_api_error( const std::string& message )
    {
      FC_ASSERT( false, "${msg}", ("msg", message) );
    }

    beekeeper_minimal::public_key_type pub_from_string( const std::string& source )
    {
      return _crypto.public_key_from_string( source, prefix );
    }

  public:
    beekeeper_api_impl( beekeeper_minimal::beekeeper& bk, beekeeper_minimal::crypto_provider& crypto,
                        beekeeper_minimal::wallet_storage& storage,
                        std::shared_ptr<mutex_handler> mtx_handler, appbase::application& app, uint64_t unlock_interval )
                      : prefix( HIVE_ADDRESS_PREFIX ),
                        ex_api( unlock_interval ), _bk( bk ), _crypto( crypto ), _storage( storage ), _mtx_handler( mtx_handler ), app( app )
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
    appbase::application& app;
};

DEFINE_API_IMPL( beekeeper_api_impl, create )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  if( !is_wallet_name_valid( args.wallet_name ) )
    throw_api_error( "Name of wallet is incorrect." );

  if( _bk.has_wallet( args.wallet_name ) )
    throw_api_error( "Wallet with name: '" + args.wallet_name + "' already exists" );

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
  auto w = find_session_wallet( args.token, args.wallet_name );
  if( !w )
    throw_api_error( "Wallet not found: " + args.wallet_name );
  if( !w->unlocked )
    throw_api_error( "Unable to lock a locked wallet" );

  _bk.lock( args.wallet_name );
  return lock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, unlock )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );

  auto w = find_session_wallet( args.token, args.wallet_name );
  const bool in_session = static_cast<bool>( w );

  if( !w )
  {
    try
    {
      _bk.open_wallet( args.token, args.wallet_name );
      w = find_session_wallet( args.token, args.wallet_name );
    }
    catch( const std::exception& )
    {
      throw_api_error( "Unable to open file" );
    }
  }

  if( w && w->unlocked && in_session )
    throw_api_error( "Wallet is already unlocked: " + args.wallet_name );

  if( ex_api.unlock_allowed() )
  {
    // If wallet is already unlocked (by another session), only validate password and attach session.
    if( w && w->unlocked && !in_session )
    {
      try
      {
        _bk.check_password( args.wallet_name, args.password );
        return unlock_return();
      }
      catch( const std::exception& )
      {
        ex_api.was_error();
        throw_api_error( "Invalid password for wallet: " + args.wallet_name );
      }
    }

    try
    {
      _bk.unlock( args.wallet_name, args.password );
    }
    catch( const std::exception& e )
    {
      ex_api.was_error();

      const std::string msg = e.what();
      if( msg.find( "Wallet is already unlocked" ) != std::string::npos )
        throw_api_error( "Wallet is already unlocked: " + args.wallet_name );
      if( msg.find( "Invalid password" ) != std::string::npos )
        throw_api_error( "Invalid password for wallet: " + args.wallet_name );

      throw_api_error( msg );
    }
  }
  else
    FC_ASSERT(false, "unlock is not accessible");

  return unlock_return();
}

DEFINE_API_IMPL( beekeeper_api_impl, import_key )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  try
  {
    auto pub_str = _bk.import_key( args.wallet_name, args.wif_key, prefix );
    return { pub_str };
  }
  catch( const std::exception& e )
  {
    throw_api_error( e.what() );
  }
}

DEFINE_API_IMPL( beekeeper_api_impl, import_keys )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  try
  {
    std::vector<std::string> pub_strs;
    pub_strs.reserve( args.wif_keys.size() );
    for( const auto& wif : args.wif_keys )
      pub_strs.push_back( _bk.import_key( args.wallet_name, wif, prefix ) );
    return { pub_strs };
  }
  catch( const std::exception& e )
  {
    throw_api_error( e.what() );
  }
}

DEFINE_API_IMPL( beekeeper_api_impl, remove_key )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
  auto pub = pub_from_string( args.public_key );
  try
  {
    _bk.remove_key( args.wallet_name, pub );
    return remove_key_return();
  }
  catch( const std::exception& e )
  {
    throw_api_error( e.what() );
  }
}

DEFINE_API_IMPL( beekeeper_api_impl, list_wallets )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );
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
  auto session_wallets = _bk.list_wallets( args.token );
  flat_set<wallet_details> result;

  std::map<std::string, bool> session_status;
  for( auto& w : session_wallets )
    session_status.emplace( w.name, w.unlocked );

  auto names = static_cast<beekeeper::file_storage&>(_storage).list_dir();
  for( auto& name : names )
  {
    auto it = session_status.find( name );
    result.insert( wallet_details{ name, it != session_status.end() ? it->second : false } );
  }
  return { result };
}

DEFINE_API_IMPL( beekeeper_api_impl, get_public_keys )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );

  flat_set<public_key_details> result;

  if( args.wallet_name.has_value() )
  {
    auto w = find_session_wallet( args.token, *args.wallet_name );
    if( !w || !w->unlocked )
      throw_api_error( "Wallet " + *args.wallet_name + " is locked" );

    auto keys = _bk.get_public_keys( *args.wallet_name );
    for( auto& [pub, kd] : keys )
      result.insert( public_key_details{ _crypto.public_key_to_string( pub, kd.second ) } );
  }
  else
  {
    // Get keys from all unlocked wallets in this session
    auto wallets = _bk.list_wallets( args.token );
    if( wallets.empty() )
      throw_api_error( "You don't have any wallet" );

    bool has_unlocked = false;
    for( auto& w : wallets )
    {
      if( w.unlocked )
      {
        has_unlocked = true;
        auto keys = _bk.get_public_keys( w.name );
        for( auto& [pub, kd] : keys )
          result.insert( public_key_details{ _crypto.public_key_to_string( pub, kd.second ) } );
      }
    }

    if( !has_unlocked )
      throw_api_error( "You don't have any unlocked wallet" );
  }

  return { result };
}

DEFINE_API_IMPL( beekeeper_api_impl, sign_digest )
{
  std::shared_lock guard( _mtx_handler->get_mutex() );

  _bk.validate_token( args.token );

  if( args.sig_digest.empty() )
    throw_api_error( "`sig_digest` can't be empty" );

  auto digest = _crypto.digest_from_hex( args.sig_digest );
  auto pub = pub_from_string( args.public_key );

  if( args.wallet_name.has_value() )
  {
    auto w = find_session_wallet( args.token, *args.wallet_name );
    if( !w )
      throw_api_error( "Public key " + args.public_key + " not found in " + *args.wallet_name + " wallet" );

    try
    {
      auto sig = _bk.sign_digest( *args.wallet_name, digest, pub, prefix );
      return { _crypto.signature_to_hex( sig ) };
    }
    catch( const std::exception& )
    {
      throw_api_error( "Public key " + args.public_key + " not found in " + *args.wallet_name + " wallet" );
    }
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

    throw_api_error( "Public key " + args.public_key + " not found in unlocked wallets" );
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

  constexpr size_t max_sessions = 64;
  if( _bk.session_count() >= max_sessions )
    throw_api_error( "Number of concurrent sessions reached a limit ==`" + std::to_string( max_sessions ) + "`" );

  try
  {
    auto token = _bk.create_session();
    return { token };
  }
  catch( const std::exception& e )
  {
    throw_api_error( e.what() );
  }
}

DEFINE_API_IMPL( beekeeper_api_impl, close_session )
{
  std::unique_lock guard( _mtx_handler->get_mutex() );

  try
  {
    _bk.validate_token( args.token );
  }
  catch( const std::exception& )
  {
    throw_api_error( "A session attached to " + args.token + " doesn't exist" );
  }

  _bk.close_session( args.token );
  if( _bk.session_count() == 0 )
    app.generate_interrupt_request();
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
