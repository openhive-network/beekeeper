#include <beekeeper/beekeeper_app.hpp>

#include <fc/value_set.hpp>
#include <fc/io/json.hpp>
#include <fc/stacktrace.hpp>

#include <hive/plugins/webserver/webserver_plugin.hpp>
#include <hive/plugins/app_status_api/app_status_api_plugin.hpp>

#include <boost/scope_exit.hpp>

namespace beekeeper {

beekeeper_app::beekeeper_app()
{
  app.init_signals_handler();
}

beekeeper_app::~beekeeper_app()
{
  timer_.reset(); // Stop timeout thread before destroying beekeeper

  if( start_loop )
  {
    ilog("beekeeper is exiting");
    app.quit( true/*log*/ );
    ilog("exited cleanly");
  }
  else
    app.quit();
}

void beekeeper_app::set_program_options()
{
  options_cli.add_options()
    ("export-keys-wallet", boost::program_options::value< std::vector<std::string> >()->composing()->multitoken(),
      "Export explicitly private keys to a local file `wallet_name.keys`. Both [name, password] are required for every wallet. By default is empty."
      "Two wallets example: --export-keys-wallet \"[\"blue-wallet\", \"PW5JViFn5gd4rt6ohk7DQMgHzQN6Z9FuMRfKoE5Ysk25mkjy5AY1b\"]\" --export-keys-wallet \"[\"green-wallet\", \"PW5KYF9Rt4ETnuP4uheHSCm9kLbCuunf6RqeKgQ8QRoxZmGeZUhhk\"]\" ")
    ;

  options_cfg.add_options()
    ("unlock-interval", boost::program_options::value<uint64_t>()->default_value( 500 ), "Protection against unlocking by bots. Every wrong `unlock` enables a delay. By default 500[ms]." )
    ("wallet-dir", bpo::value<std::string>()->default_value("."),
      "The path of the wallet files (absolute path or relative to application data dir)")
    ("unlock-timeout", bpo::value<uint64_t>()->default_value(900),
      "Timeout for unlocked wallet in seconds (default 900 (15 minutes))."
      "Wallets will be automatically locked after specified number of seconds of inactivity."
      "Activity is defined as any wallet command e.g. list-wallets.")
    ("backtrace", bpo::value<std::string>()->default_value( "yes" ), "Whether to print backtrace on SIGSEGV" )
    ;
}

std::string beekeeper_app::check_version()
{
  std::string _version = "{\"version\":\"";
  _version += utility::get_revision();
  _version += "\"}";

  return _version;
}

struct keys_container
{
  std::string public_key;
  std::string private_key;
};

uint32_t beekeeper_app::save_keys( const std::string& wallet_name, const std::string& wallet_password )
{
  uint32_t _result = initialization_result::fail;

  if( wallet_name.empty() || wallet_password.empty() )
    return initialization_result::ok;

  const std::string _filename = wallet_name + ".keys";

  ilog( "*****Saving keys into `${_filename}` file*****", (_filename) );

  ilog( "Create a session" );
  std::string _token = bk_->create_session();

  auto _save_keys = [&]()
  {
    ilog( "Open the wallet" );
    bk_->open_wallet( _token, wallet_name );

    ilog( "Unlock the wallet" );
    bk_->unlock( wallet_name, wallet_password );

    ilog( "Get keys" );
    auto _keys = bk_->get_public_keys( wallet_name );

    std::vector<keys_container> _v;
    std::transform( _keys.begin(), _keys.end(), std::back_inserter( _v ),
    [this]( const beekeeper_minimal::keys_map::value_type& item )
    {
      return keys_container{
        crypto_->public_key_to_string( item.first, item.second.second ),
        crypto_->key_to_wif( item.second.first )
      };
    } );

    ilog( "Save keys into `${_filename}` file", (_filename) );
    fc::path _file( _filename );
    fc::json::save_to_file( _v, _file );
  };

  auto _finish = [this, &_token, &wallet_name]()
  {
    ilog( "Lock the wallet" );
    bk_->lock( wallet_name );

    ilog( "Close a session" );
    bk_->close_session( _token );
  };

  auto _exec_action = [&_result]( std::function<void()>&& call )
  {
    try
    {
      call();
      _result = initialization_result::ok;
    }
    catch ( const boost::exception& e )
    {
      elog( boost::diagnostic_information(e) );
    }
    catch ( const fc::exception& e )
    {
      elog( e.to_detail_string() );
    }
    catch ( const std::exception& e )
    {
      elog( e.what() );
    }
    catch ( ... )
    {
      elog( "Unknown error" );
    }
  };

  BOOST_SCOPE_EXIT(&bk_, &_exec_action, &_finish, &_result)
  {
    _exec_action( _finish );

    if ( _result == initialization_result::ok )
      ilog( "*****Keys have been saved*****" );
    else
      elog( "*****Saving keys failed*****" );

  } BOOST_SCOPE_EXIT_END

  _exec_action( _save_keys );

  return _result;
}

uint32_t beekeeper_app::initialize_program_options()
{
  try {
      const auto& _args = app.get_args();

      FC_ASSERT( _args.count("unlock-interval") );
      unlock_interval = _args.at("unlock-interval").as<uint64_t>();
      ilog("Options are set.");

      FC_ASSERT( _args.count("wallet-dir") );
      boost::filesystem::path _dir( _args.at("wallet-dir").as<std::string>() );
      if(_dir.is_relative() )
          _dir = app.data_dir() / _dir;
      if( !bfs::exists( _dir ) )
          bfs::create_directories( _dir );

      FC_ASSERT( _args.count("unlock-timeout") );
      auto _timeout = _args.at("unlock-timeout").as<uint64_t>();

      // Create core_minimal objects
      instance = std::make_shared<beekeeper_instance>( app, _dir );
      mtx_handler = std::make_shared<mutex_handler>();

      crypto_ = std::make_unique<beekeeper_minimal::fc_crypto_provider>();
      storage_ = std::make_unique<beekeeper::file_storage>( _dir );
      bk_ = std::make_unique<beekeeper_minimal::beekeeper>( *crypto_, *storage_, static_cast<uint32_t>(_timeout) );

      // Start timeout thread
      timer_ = std::make_unique<beekeeper::time_manager>( [this]()
      {
        std::shared_lock guard( mtx_handler->get_mutex() );
        bk_->check_timeout();
      });

      FC_ASSERT( _args.count("backtrace") );
      if( _args.at( "backtrace" ).as<std::string>() == "yes" )
      {
        fc::print_stacktrace_on_segfault();
        ilog( "Backtrace on segfault is enabled." );
      }

      return save_keys( _args );

  } FC_LOG_AND_RETHROW()
}

uint32_t beekeeper_app::initialize( int argc, char** argv )
{
  app.add_program_options( options_cli, options_cfg );
  app.set_app_name( "beekeeper" );
  app.set_version_string( check_version() );

  app.register_plugin<hive::plugins::webserver::webserver_plugin>();
  app.register_plugin<hive::plugins::app_status_api::app_status_api_plugin>();

  auto initializationResult = app.initialize<
                                hive::plugins::webserver::webserver_plugin,
                                hive::plugins::app_status_api::app_status_api_plugin>
                              ( argc, argv );
  start_loop = initializationResult.should_start_loop();

  if( !initializationResult.should_start_loop() )
    return initializationResult.get_result_code();
  else
  {
    auto _initialization = initialize_program_options();
    if( _initialization == initialization_result::fail )
    {
      start_loop = false;
      return _initialization;
    }

    if( instance->is_instance_started() )
    {
      api_ptr = std::make_unique<beekeeper::beekeeper_wallet_api>( *bk_, *crypto_, *storage_, mtx_handler, app, unlock_interval );
      instance->get_app().status.save_status( "beekeeper is starting" );
    }
    else
    {
      instance->get_app().status.save_status( "opening beekeeper failed");
    }

    return _initialization;
  }
}

void beekeeper_app::start()
{
  auto& _webserver_plugin = app.get_plugin<hive::plugins::webserver::webserver_plugin>();

  app.startup();

  //Launch webserver only when all plugins are initialized at startup.
  if( !app.is_interrupt_request() )
  {
    _webserver_plugin.start_webserver();
    instance->get_app().status.save_status( "beekeeper is ready" );
  }

  ilog("beekeeper is waiting");
  app.wait( true/*log*/ );
  ilog("waiting is finished");
}

uint32_t beekeeper_app::run( int argc, char** argv )
{
  set_program_options();

  auto _result = initialize( argc, argv );

  if( start_loop )
    start();

  return _result;
}

uint32_t beekeeper_app::save_keys( const boost::program_options::variables_map& args )
{
  uint32_t _result = initialization_result::ok;

  using _strings_pair_type = std::pair< std::string, std::string >;
  fc::flat_map< std::string, std::string > _items;
  fc::load_value_set<_strings_pair_type>( args, "export-keys-wallet", _items );

  for( auto& item : _items )
  {
    _result = save_keys( item.first, item.second );
    if( _result == initialization_result::fail )
      break;
  }

  if( args.count("export-keys-wallet") )
    start_loop = false;

  return _result;
}

uint32_t beekeeper_app::init( int argc, char** argv )
{
  uint32_t _result = initialization_result::fail;
  try
  {
    _result = run( argc, argv );
  }
  catch ( const boost::exception& e )
  {
    elog(boost::diagnostic_information(e));
  }
  catch ( const fc::exception& e )
  {
    elog(e.to_detail_string());
  }
  catch ( const std::exception& e )
  {
    elog(e.what());
  }
  catch ( ... )
  {
    elog("unknown exception");
  }

  return _result;
}

}

namespace fc
{
  void to_variant( const beekeeper::keys_container& var, fc::variant& vo )
  {
    variant v = mutable_variant_object( "public_key", var.public_key )( "private_key", var.private_key );
    vo = v;
  }
}

FC_REFLECT( beekeeper::keys_container, (public_key)(private_key) )
