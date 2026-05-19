#pragma once

#include <core_minimal/crypto_provider.hpp>

#include <rust/cxx.h>

namespace cpp {
	struct RustCryptoProtocol;
}

namespace beekeeper_rs {
	class rust_crypto_provider final : public beekeeper_minimal::crypto_provider
	{
	public:
		explicit rust_crypto_provider(rust::Box<cpp::RustCryptoProtocol> impl);
		~rust_crypto_provider() override;

	private:
		rust::Box<cpp::RustCryptoProtocol> impl_;
	};
}
