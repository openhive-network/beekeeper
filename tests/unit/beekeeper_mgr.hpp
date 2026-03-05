#ifdef IS_TEST_NET

#include <fc/filesystem.hpp>

#include <core_minimal/beekeeper.hpp>
#include <fc_crypto_bridge/fc_crypto_provider.hpp>
#include <beekeeper/file_storage.hpp>

#include <boost/filesystem.hpp>

namespace test_utils
{
  struct beekeeper_mgr
  {
    boost::filesystem::path dir;
    beekeeper_minimal::fc_crypto_provider crypto;
    std::unique_ptr<beekeeper::file_storage> storage;

    beekeeper_mgr()
      : dir( boost::filesystem::current_path() / "beekeeper-storage" )
    {
      boost::filesystem::create_directories( dir );
      storage = std::make_unique<beekeeper::file_storage>( dir );
    }

    void remove_wallets()
    {
      boost::filesystem::directory_iterator _end_itr;

      for( boost::filesystem::directory_iterator itr( dir ); itr != _end_itr; ++itr )
        boost::filesystem::remove_all( itr->path() );
    }

    void remove_wallet( const std::string& wallet_name )
    {
      try
      {
        auto _wallet_name = wallet_name + ".wallet";
        boost::filesystem::remove( dir / _wallet_name );
      }
      catch(...)
      {
      }
    }

    bool exists_wallet( const std::string& wallet_name )
    {
      auto _wallet_name = wallet_name + ".wallet";
      return boost::filesystem::exists( dir / _wallet_name );
    }

    beekeeper_minimal::beekeeper create_beekeeper( uint32_t unlock_timeout = 900 )
    {
      return beekeeper_minimal::beekeeper( crypto, *storage, unlock_timeout );
    }
  };
}

#endif
