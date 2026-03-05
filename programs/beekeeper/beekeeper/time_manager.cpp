#include <beekeeper/time_manager.hpp>

namespace beekeeper {

time_manager::time_manager( std::function<void()> check_timeout )
  : check_timeout_fn( std::move(check_timeout) )
{
  lock_thread = std::make_unique<std::thread>( [this]()
    {
      while( !stop_requested.load() )
      {
        check_timeout_fn();
        std::unique_lock<std::mutex> lock( mtx );
        cv.wait_for( lock, std::chrono::milliseconds(200), [this]() { return stop_requested.load(); } );
      }
    } );
}

time_manager::~time_manager()
{
  {
    std::lock_guard<std::mutex> lock( mtx );
    stop_requested.store( true );
  }
  cv.notify_one();
  lock_thread->join();
}

} //beekeeper
