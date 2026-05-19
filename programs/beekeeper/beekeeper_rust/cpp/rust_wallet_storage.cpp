#include <beekeeper_rs/rust_wallet_storage.hpp>

namespace beekeeper_rs {
	rust_wallet_storage::rust_wallet_storage(
		rust::Box<cpp::RustStorageProtocol> impl
	) : impl_(std::move(impl)) {}

	rust_wallet_storage::~rust_wallet_storage() = default;
}
