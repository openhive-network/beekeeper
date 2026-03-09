#include <beekeeper/file_storage.hpp>

#include <boost/filesystem.hpp>

#include <fstream>
#include <stdexcept>

namespace bfs = boost::filesystem;

namespace beekeeper {

file_storage::file_storage(const boost::filesystem::path& wallet_dir)
  : wallet_dir_(wallet_dir)
{
}

boost::filesystem::path file_storage::wallet_path(const std::string& name) const
{
  return wallet_dir_ / (name + ".wallet");
}

void file_storage::save(const std::string& name, const std::vector<char>& buffer)
{
  auto path = wallet_path(name);
  std::ofstream ofs(path.string(), std::ios::binary | std::ios::trunc);
  if (!ofs)
    throw std::runtime_error("Cannot write wallet file: " + path.string());

  ofs.write(buffer.data(), buffer.size());
  if (!ofs)
    throw std::runtime_error("Error writing wallet file: " + path.string());
}

std::vector<char> file_storage::load(const std::string& name)
{
  auto path = wallet_path(name);
  if (!bfs::exists(path))
    throw std::runtime_error("Wallet file not found: " + path.string());

  std::ifstream ifs(path.string(), std::ios::binary | std::ios::ate);
  if (!ifs)
    throw std::runtime_error("Cannot read wallet file: " + path.string());

  auto size = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  std::vector<char> buffer(size);
  if (!ifs.read(buffer.data(), size))
    throw std::runtime_error("Error reading wallet file: " + path.string());

  return buffer;
}

bool file_storage::scan_dir(const std::string& wallet_name)
{
  return bfs::exists(wallet_path(wallet_name));
}

std::vector<std::string> file_storage::list_dir()
{
  std::vector<std::string> result;

  if (!bfs::exists(wallet_dir_) || !bfs::is_directory(wallet_dir_))
    return result;

  for (auto& entry : bfs::directory_iterator(wallet_dir_))
  {
    if (bfs::is_regular_file(entry.status()) && entry.path().extension() == ".wallet")
      result.push_back(entry.path().stem().string());
  }

  return result;
}

} // namespace beekeeper
