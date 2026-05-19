#include <beekeeper_rs/rust_crypto_provider.hpp>

namespace beekeeper_rs {
	rust_crypto_provider::rust_crypto_provider(
		rust::Box<cpp::RustCryptoProtocol> impl
	) : impl_(std::move(impl)) {}

	rust_crypto_provider::~rust_crypto_provider() = default;
}
