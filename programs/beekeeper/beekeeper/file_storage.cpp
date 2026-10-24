#include <beekeeper/file_storage.hpp>

#include <boost/filesystem.hpp>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

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
  write_wallet_file(name, buffer, false);
}

bool file_storage::try_save(const std::string& name, const std::vector<char>& buffer)
{
  try
  {
    write_wallet_file(name, buffer, true);
    return true;
  }
  catch (...)
  {
    return false;
  }
}

void file_storage::write_wallet_file(const std::string& name, const std::vector<char>& buffer, bool durable)
{
  // Write a new owner-only temp file and rename it over the target, so an
  // interrupted save cannot leave a truncated wallet: the wallet file is the
  // only copy of the user's keys. O_EXCL after removing a stale temp file means
  // a symlink planted at the temp path is never followed. A durable write also
  // flushes the file and the directory, so a power loss cannot lose it either;
  // regular saves skip that, as an fsync per imported key makes bulk imports
  // tens of times slower on some filesystems.
  auto path = wallet_path(name);
  auto tmp_path = path;
  tmp_path += ".tmp";
  const auto tmp = tmp_path.string();

  ::unlink(tmp.c_str());
  int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR);
  if (fd < 0)
    throw std::runtime_error("Cannot write wallet file: " + tmp + ": " + std::strerror(errno));

  bool written = true;
  for (size_t done = 0; written && done < buffer.size(); )
  {
    auto n = ::write(fd, buffer.data() + done, buffer.size() - done);
    if (n < 0 && errno == EINTR)
      continue;
    written = n > 0;
    if (written)
      done += static_cast<size_t>(n);
  }
  written = written && (!durable || ::fsync(fd) == 0);
  written = ::close(fd) == 0 && written;
  if (!written)
  {
    ::unlink(tmp.c_str());
    throw std::runtime_error("Error writing wallet file: " + tmp);
  }

  if (::rename(tmp.c_str(), path.string().c_str()) != 0)
  {
    ::unlink(tmp.c_str());
    throw std::runtime_error("Cannot replace wallet file: " + path.string());
  }

  if (durable)
  {
    // Persist the rename itself; a failure here does not make the save fail.
    int dir_fd = ::open(wallet_dir_.string().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dir_fd >= 0)
    {
      ::fsync(dir_fd);
      ::close(dir_fd);
    }
  }
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
