#pragma once

#include <core_minimal/wallet_storage.hpp>

#include <rust/cxx.h>

namespace cpp {
	struct RustStorageProtocol;
}

namespace beekeeper_rs {
	class rust_wallet_storage final : public beekeeper_minimal::wallet_storage
	{
	public:
		explicit rust_wallet_storage(rust::Box<cpp::RustStorageProtocol> impl);
		~rust_wallet_storage() override;

	private:
		rust::Box<cpp::RustStorageProtocol> impl_;
	};
}
