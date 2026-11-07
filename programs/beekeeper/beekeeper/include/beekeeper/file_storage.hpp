#pragma once

#include <core_minimal/wallet_storage.hpp>

#include <boost/filesystem/path.hpp>

#include <functional>

namespace beekeeper {

/// Implements wallet_storage using Boost.Filesystem.
/// Wallets are stored as {wallet_dir}/{name}.wallet files.
class file_storage final : public beekeeper_minimal::wallet_storage
{
public:
  /// Flushes a file descriptor to disk; returns 0 on success, like fsync(2).
  using sync_function = std::function<int(int)>;

  explicit file_storage(const boost::filesystem::path& wallet_dir);
  /// Uses `sync` instead of fsync(2) to flush wallet files and the wallet directory.
  file_storage(const boost::filesystem::path& wallet_dir, sync_function sync);

  /// Wallet names consist of ASCII letters, digits and '.', '_', '-', '@', so {name}.wallet
  /// always names a file directly in the wallet directory. save() and load() refuse other
  /// names, scan_dir() reports no wallet for them.
  static bool is_valid_name(const std::string& name);

  void save(const std::string& name, const std::vector<char>& buffer) override;
  std::vector<char> load(const std::string& name) override;
  bool scan_dir(const std::string& wallet_name) override;

  /// Like save(), but also flushes the file and the wallet directory to disk.
  void save_durable(const std::string& name, const std::vector<char>& buffer) override;

  /// Used for the rewrite of a legacy wallet on unlock; unlike save() it also
  /// flushes the file to disk, as nobody asked for that rewrite.
  bool try_save(const std::string& name, const std::vector<char>& buffer) override;

  /// Return names of all wallets currently in storage (native daemon only).
  std::vector<std::string> list_dir();

private:
  boost::filesystem::path wallet_dir_;
  sync_function sync_;

  boost::filesystem::path wallet_path(const std::string& name) const;
  void write_wallet_file(const std::string& name, const std::vector<char>& buffer, bool durable);
};

} // namespace beekeeper
