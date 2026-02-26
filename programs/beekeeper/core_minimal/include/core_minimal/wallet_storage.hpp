#pragma once

#include <string>
#include <vector>

namespace beekeeper_minimal {

/// Abstract filesystem hooks. The consumer provides the implementation.
struct wallet_storage
{
  virtual ~wallet_storage() = default;

  /// Persist a wallet blob at the given path.
  virtual void save(const std::string& path, const std::vector<char>& buffer) = 0;

  /// Retrieve a previously-persisted wallet blob. Throws if not found.
  virtual std::vector<char> load(const std::string& path) = 0;

  /// Return names of all wallets currently in storage.
  virtual std::vector<std::string> list_dir() = 0;
};

} // namespace beekeeper_minimal
