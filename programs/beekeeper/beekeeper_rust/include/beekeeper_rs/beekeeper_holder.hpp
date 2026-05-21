#pragma once

#include <beekeeper_rs/rust_wallet_storage.hpp>

#include <core_minimal/beekeeper.hpp>
#include <core_minimal/wallet_storage.hpp>
#include <fc_crypto_bridge/fc_crypto_provider.hpp>

#include <rust/cxx.h>

#include <cstdint>
#include <memory>

namespace cpp {
	struct RustStorageProtocol;
	struct WalletDetails;
}

namespace beekeeper_rs {
	class beekeeper_holder
	{
	public:
		/// Persistent backend (Rust-backed file storage).
		beekeeper_holder(
			rust::Box<cpp::RustStorageProtocol> storage_impl,
			uint32_t unlock_timeout_sec
		);
		/// In-memory backend (core_minimal::memory_storage). Wallets vanish at destruction.
		explicit beekeeper_holder(uint32_t unlock_timeout_sec);
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
		void         lock(rust::Str name);
		void         lock_all();

		void         sync_storage();
		void         close_storage();

		rust::Vec<cpp::WalletDetails> list_wallets(rust::Str token) const;

		rust::String import_key(rust::Str name, rust::Str wif_key, rust::Str prefix);
		void         remove_key(rust::Str name, rust::Str public_key, rust::Str prefix);
		bool         has_matching_private_key(rust::Str name, rust::Str public_key, rust::Str prefix) const;
		rust::String sign_digest(rust::Str name, rust::Str digest_hex, rust::Str public_key, rust::Str prefix);
		rust::Vec<rust::String> get_public_keys(rust::Str name, rust::Str prefix) const;
		rust::String encrypt_data(
			rust::Str name,
			rust::Str from_key,
			rust::Str to_key,
			rust::Str content,
			rust::Str prefix,
			uint64_t nonce
		);
		rust::String decrypt_data(
			rust::Str name,
			rust::Str from_key,
			rust::Str to_key,
			rust::Str encrypted_content,
			rust::Str prefix
		);

	private:
		/// FC-backed crypto provider. Same implementation the native daemon
		/// uses, so signatures, ECDH, AES, hashes are byte-identical with the
		/// `@hiveio/beekeeper` package.
		std::unique_ptr<beekeeper_minimal::fc_crypto_provider> crypto_provider_;
		/// Persistent backend; null in in-memory mode.
		std::unique_ptr<rust_wallet_storage>                   rust_storage_;
		/// In-memory backend; null in persistent mode.
		std::unique_ptr<beekeeper_minimal::wallet_storage>     mem_storage_;
		std::unique_ptr<beekeeper_minimal::beekeeper>          bk_;
	};

	std::unique_ptr<beekeeper_holder> new_beekeeper_holder(
		rust::Box<cpp::RustStorageProtocol> storage_impl,
		uint32_t unlock_timeout_sec
	);

	std::unique_ptr<beekeeper_holder> new_beekeeper_holder_in_memory(
		uint32_t unlock_timeout_sec
	);
}
