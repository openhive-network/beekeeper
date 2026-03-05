#pragma once

#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>

namespace beekeeper {

/// Background thread that periodically calls a check_timeout callback.
class time_manager
{
  private:

    std::atomic<bool> stop_requested{false};

    std::unique_ptr<std::thread> lock_thread;
    std::mutex mtx;
    std::condition_variable cv;

    std::function<void()> check_timeout_fn;

  public:

    /// @param check_timeout  Callback invoked every ~200ms to enforce auto-lock.
    explicit time_manager( std::function<void()> check_timeout );
    ~time_manager();
};

} //beekeeper
