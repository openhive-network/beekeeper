#pragma once

#include <beekeeper_rs/rust_crypto_provider.hpp>
#include <beekeeper_rs/rust_wallet_storage.hpp>

#include <core_minimal/beekeeper.hpp>

#include <rust/cxx.h>

#include <cstdint>
#include <memory>

namespace cpp {
	struct RustCryptoProtocol;
	struct RustStorageProtocol;
}

namespace beekeeper_rs {
	class beekeeper_holder
	{
	public:
		beekeeper_holder(
			rust::Box<cpp::RustCryptoProtocol> crypto_impl,
			rust::Box<cpp::RustStorageProtocol> storage_impl,
			uint32_t unlock_timeout_sec
		);
		~beekeeper_holder();

		rust::String create_session();
		void         close_session(rust::Str token);
		bool         has_wallet(rust::Str name) const;
		rust::String create_wallet(
			rust::Str token,
			rust::Str name,
			rust::Str password,
			bool is_temporary
		);
		void         open_wallet(rust::Str token, rust::Str name);
		void         close_wallet(rust::Str name);
		void         unlock(rust::Str name, rust::Str password);
		void         lock_all();
		void         set_timeout(uint32_t seconds);

	private:
		std::unique_ptr<rust_crypto_provider>          crypto_;
		std::unique_ptr<rust_wallet_storage>           storage_;
		std::unique_ptr<beekeeper_minimal::beekeeper>  bk_;
	};

	std::unique_ptr<beekeeper_holder> new_beekeeper_holder(
		rust::Box<cpp::RustCryptoProtocol> crypto_impl,
		rust::Box<cpp::RustStorageProtocol> storage_impl,
		uint32_t unlock_timeout_sec
	);
}
