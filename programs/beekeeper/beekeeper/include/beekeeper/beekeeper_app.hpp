#pragma once

#include <appbase/application.hpp>

#include <beekeeper/api_types.hpp>
#include <beekeeper/beekeeper_instance.hpp>
#include <beekeeper/beekeeper_wallet_api.hpp>
#include <beekeeper/mutex_handler.hpp>
#include <beekeeper/file_storage.hpp>
#include <beekeeper/time_manager.hpp>

#include <core_minimal/beekeeper.hpp>
#include <fc_crypto_bridge/fc_crypto_provider.hpp>

#include <boost/program_options.hpp>

namespace beekeeper {

namespace bpo = boost::program_options;
namespace bfs = boost::filesystem;

class beekeeper_app
{
  private:

    bool start_loop = true;

    uint64_t unlock_interval = 0;

    std::shared_ptr<beekeeper_instance> instance;

    std::unique_ptr<beekeeper::beekeeper_wallet_api> api_ptr;

    appbase::application app;

    std::shared_ptr<mutex_handler> mtx_handler;

    // core_minimal objects
    std::unique_ptr<beekeeper_minimal::fc_crypto_provider> crypto_;
    std::unique_ptr<beekeeper::file_storage> storage_;
    std::unique_ptr<beekeeper_minimal::beekeeper> bk_;
    std::unique_ptr<beekeeper::time_manager> timer_;

    bpo::options_description options_cli;
    bpo::options_description options_cfg;

    std::string check_version();
    uint32_t save_keys( const std::string& wallet_name, const std::string& wallet_password );

    void set_program_options();
    uint32_t initialize( int argc, char** argv );
    void start();

    uint32_t initialize_program_options();
    uint32_t save_keys( const bpo::variables_map& args );

    uint32_t run( int argc, char** argv );

  public:

    beekeeper_app();
    ~beekeeper_app();

    uint32_t init( int argc, char** argv );
};

}
