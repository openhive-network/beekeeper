#ifdef IS_TEST_NET

#include <fc/filesystem.hpp>

#include <core/wallet_content_handler.hpp>
#include <core/beekeeper_wallet_manager.hpp>
#include <core/session_manager_base.hpp>
#include <core/time_manager_base.hpp>
#include <core/beekeeper_instance_base.hpp>

#include <beekeeper/mutex_handler.hpp>

#include <appbase/application.hpp>

#include <thread>
#include <atomic>
#include <condition_variable>

using beekeeper_wallet_manager  = beekeeper::beekeeper_wallet_manager;
using wallet_content_handler    = beekeeper::wallet_content_handler;
using session_manager_base      = beekeeper::session_manager_base;

namespace test_utils
{
  /// Thread-enabled time manager for tests — calls run() every 200ms.
  class test_time_manager : public beekeeper::time_manager_base
  {
    std::atomic<bool> stop_requested{false};
    std::unique_ptr<std::thread> lock_thread;
    std::mutex mtx;
    std::condition_variable cv;

  public:
    test_time_manager()
    {
      lock_thread = std::make_unique<std::thread>([this]() {
        while (!stop_requested.load()) {
          run();
          std::unique_lock<std::mutex> lock(mtx);
          cv.wait_for(lock, std::chrono::milliseconds(200), [this]() { return stop_requested.load(); });
        }
      });
    }

    ~test_time_manager() override
    {
      {
        std::lock_guard<std::mutex> lock(mtx);
        stop_requested.store(true);
      }
      cv.notify_one();
      lock_thread->join();
    }

    void add(const std::string& token, beekeeper::types::lock_method_type&& lock_method) override
    {
      std::lock_guard<std::mutex> guard(mtx);
      time_manager_base::add(token, std::move(lock_method));
    }

    void change(const std::string& token, const beekeeper::types::timepoint_t& time, bool refresh_only_active) override
    {
      std::lock_guard<std::mutex> guard(mtx);
      time_manager_base::change(token, time, refresh_only_active);
    }

    void run() override
    {
      std::lock_guard<std::mutex> guard(mtx);
      time_manager_base::run();
    }

    void run(const std::string& token) override
    {
      std::lock_guard<std::mutex> guard(mtx);
      time_manager_base::run(token);
    }

    void close(const std::string& token) override
    {
      std::lock_guard<std::mutex> guard(mtx);
      time_manager_base::close(token);
    }
  };

  class test_session_manager : public session_manager_base
  {
  public:
    test_session_manager()
    {
      time = std::make_shared<test_time_manager>();
    }
  };

  struct beekeeper_mgr
  {
    fc::path dir;

    beekeeper_mgr()
      : dir( fc::current_path() / "beekeeper-storage" )
    {
      fc::create_directories( dir );
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
        fc::remove( dir / _wallet_name );
      }
      catch(...)
      {
      }
    }

    bool exists_wallet( const std::string& wallet_name )
    {
      auto _wallet_name = wallet_name + ".wallet";
      return fc::exists( dir / _wallet_name );
    }

    beekeeper_wallet_manager create_wallet( appbase::application& app, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit, std::function<void()>&& method = [](){}, std::shared_ptr<beekeeper::mutex_handler> mtx_handler = std::make_shared<beekeeper::mutex_handler>() )
    {
      return beekeeper_wallet_manager(  std::make_shared<test_session_manager>(),
                                        std::make_shared<beekeeper::beekeeper_instance_base>(),
                                        dir,
                                        cmd_unlock_timeout,
                                        cmd_session_limit,
                                        std::move( method )
                                      );
    }

    std::shared_ptr<beekeeper_wallet_manager> create_wallet_ptr( appbase::application& app, uint64_t cmd_unlock_timeout, uint32_t cmd_session_limit, std::function<void()>&& method = [](){}, std::shared_ptr<beekeeper::mutex_handler> mtx_handler = std::make_shared<beekeeper::mutex_handler>() )
    {
      return std::shared_ptr<beekeeper_wallet_manager>( new beekeeper_wallet_manager( std::make_shared<test_session_manager>(),
                                                                                      std::make_shared<beekeeper::beekeeper_instance_base>(),
                                                                                      dir,
                                                                                      cmd_unlock_timeout,
                                                                                      cmd_session_limit,
                                                                                      std::move( method )
                                                                                    ) );
    }

  };
}

#endif
