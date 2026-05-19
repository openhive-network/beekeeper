#include <beekeeper_rs/rust_wallet_storage.hpp>

#include "beekeeper_rust/src/lib.rs.h"

namespace beekeeper_rs {
	rust_wallet_storage::rust_wallet_storage(
		rust::Box<cpp::RustStorageProtocol> impl
	) : impl_(std::move(impl)) {}

	rust_wallet_storage::~rust_wallet_storage() = default;

	std::vector<char> rust_wallet_storage::load(const std::string& path)
	{
		auto bytes = impl_->cpp_load(rust::Str{path});
		return std::vector<char>(bytes.begin(), bytes.end());
	}
}
