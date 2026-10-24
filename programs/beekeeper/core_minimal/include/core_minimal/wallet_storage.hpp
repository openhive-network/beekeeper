#pragma once

#include <optional>
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

  /// Check whether a wallet with the given name exists in storage.
  virtual bool scan_dir(const std::string& wallet_name) = 0;

  /// Like save(), but reports failure instead of throwing. Used where a failed
  /// write must not fail the operation (converting a legacy wallet on unlock).
  /// Storages whose save() can fail in a way C++ cannot catch (a JS exception
  /// in the WASM build) override it.
  virtual bool try_save(const std::string& path, const std::vector<char>& buffer)
  {
    try
    {
      save(path, buffer);
      return true;
    }
    catch (...)
    {
      return false;
    }
  }

  /// Like load(), but returns nothing instead of throwing.
  virtual std::optional<std::vector<char>> try_load(const std::string& path)
  {
    try
    {
      return load(path);
    }
    catch (...)
    {
      return std::nullopt;
    }
  }
};

} // namespace beekeeper_minimal
