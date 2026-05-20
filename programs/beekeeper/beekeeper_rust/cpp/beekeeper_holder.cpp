#include <beekeeper_rs/beekeeper_holder.hpp>

#include "beekeeper_rust/src/lib.rs.h"

#include <core_minimal/memory_storage.hpp>

#include <string>

namespace beekeeper_rs {
	beekeeper_holder::beekeeper_holder(
		rust::Box<cpp::RustCryptoProtocol> crypto_impl,
		rust::Box<cpp::RustStorageProtocol> storage_impl,
		uint32_t unlock_timeout_sec
	) : crypto_prims_(std::make_unique<rust_crypto_primitives>(std::move(crypto_impl))),
	    crypto_provider_(std::make_unique<beekeeper_minimal::crypto_provider_impl>(*crypto_prims_)),
	    rust_storage_(std::make_unique<rust_wallet_storage>(std::move(storage_impl))),
	    mem_storage_(nullptr),
	    bk_(std::make_unique<beekeeper_minimal::beekeeper>(*crypto_provider_, *rust_storage_, unlock_timeout_sec))
	{}

	beekeeper_holder::beekeeper_holder(
		rust::Box<cpp::RustCryptoProtocol> crypto_impl,
		uint32_t unlock_timeout_sec
	) : crypto_prims_(std::make_unique<rust_crypto_primitives>(std::move(crypto_impl))),
	    crypto_provider_(std::make_unique<beekeeper_minimal::crypto_provider_impl>(*crypto_prims_)),
	    rust_storage_(nullptr),
	    mem_storage_(std::make_unique<beekeeper_minimal::memory_storage>()),
	    bk_(std::make_unique<beekeeper_minimal::beekeeper>(*crypto_provider_, *mem_storage_, unlock_timeout_sec))
	{}

	beekeeper_holder::~beekeeper_holder() = default;

	rust::String beekeeper_holder::create_session() {
		return bk_->create_session();
	}

	void beekeeper_holder::close_session(rust::Str token) {
		bk_->close_session(std::string(token));
	}

	bool beekeeper_holder::has_wallet(rust::Str name) const {
		return bk_->has_wallet(std::string(name));
	}

	rust::String beekeeper_holder::create_wallet(rust::Str token, rust::Str name, rust::Str password, bool is_temporary) {
		return bk_->create_wallet(std::string(token), std::string(name), std::string(password), is_temporary);
	}

	void beekeeper_holder::open_wallet(rust::Str token, rust::Str name) {
		bk_->open_wallet(std::string(token), std::string(name));
	}

	void beekeeper_holder::close_wallet(rust::Str name) {
		bk_->close_wallet(std::string(name));
	}

	void beekeeper_holder::unlock(rust::Str name, rust::Str password) {
		bk_->unlock(std::string(name), std::string(password));
	}

	void beekeeper_holder::lock(rust::Str name) {
		bk_->lock(std::string(name));
	}

	void beekeeper_holder::lock_all() {
		bk_->lock_all();
	}

	void beekeeper_holder::set_timeout(uint32_t seconds) {
		bk_->set_timeout(seconds);
	}

	void beekeeper_holder::sync_storage() {
		if (rust_storage_) rust_storage_->sync();
	}

	void beekeeper_holder::close_storage() {
		if (rust_storage_) rust_storage_->close();
	}

	rust::Vec<cpp::WalletDetails>
	beekeeper_holder::list_wallets(rust::Str token) const {
		auto wds = bk_->list_wallets(std::string(token));
		rust::Vec<cpp::WalletDetails> result;
		for (const auto& wd : wds) {
			cpp::WalletDetails out{rust::String(wd.name), wd.unlocked};
			result.push_back(std::move(out));
		}
		return result;
	}

	rust::String beekeeper_holder::import_key(rust::Str name, rust::Str wif_key, rust::Str prefix) {
		return bk_->import_key(std::string(name), std::string(wif_key), std::string(prefix));
	}

	void beekeeper_holder::remove_key(rust::Str name, rust::Str public_key, rust::Str prefix) {
		auto pk = crypto_provider_->public_key_from_string(std::string(public_key), std::string(prefix));
		bk_->remove_key(std::string(name), pk);
	}

	bool beekeeper_holder::has_matching_private_key(rust::Str name, rust::Str public_key, rust::Str prefix) const {
		auto pk = crypto_provider_->public_key_from_string(std::string(public_key), std::string(prefix));
		return bk_->has_private_key(std::string(name), pk);
	}

	rust::String beekeeper_holder::sign_digest(rust::Str name, rust::Str digest_hex, rust::Str public_key, rust::Str prefix) {
		auto digest = crypto_provider_->digest_from_hex(std::string(digest_hex));
		auto pk = crypto_provider_->public_key_from_string(std::string(public_key), std::string(prefix));
		auto sig = bk_->sign_digest(std::string(name), digest, pk, std::string(prefix));
		return crypto_provider_->signature_to_hex(sig);
	}

	rust::Vec<rust::String>
	beekeeper_holder::get_public_keys(rust::Str name, rust::Str prefix) const {
		auto keys = bk_->get_public_keys(std::string(name));
		rust::Vec<rust::String> result;
		for (const auto& [pubkey, _] : keys) {
			result.push_back(crypto_provider_->public_key_to_string(pubkey, std::string(prefix)));
		}
		return result;
	}

	rust::String beekeeper_holder::encrypt_data(
		rust::Str name,
		rust::Str from_key,
		rust::Str to_key,
		rust::Str content,
		rust::Str prefix,
		uint64_t nonce
	) {
		auto prefix_s = std::string(prefix);
		auto from = crypto_provider_->public_key_from_string(std::string(from_key), prefix_s);
		auto to   = crypto_provider_->public_key_from_string(std::string(to_key),   prefix_s);
		return bk_->encrypt_data(std::string(name), from, to, std::string(content), prefix_s, nonce);
	}

	rust::String beekeeper_holder::decrypt_data(
		rust::Str name,
		rust::Str from_key,
		rust::Str to_key,
		rust::Str encrypted_content,
		rust::Str prefix
	) {
		auto prefix_s = std::string(prefix);
		auto from = crypto_provider_->public_key_from_string(std::string(from_key), prefix_s);
		auto to   = crypto_provider_->public_key_from_string(std::string(to_key),   prefix_s);
		return bk_->decrypt_data(std::string(name), from, to, std::string(encrypted_content), prefix_s);
	}

	std::unique_ptr<beekeeper_holder> new_beekeeper_holder(
		rust::Box<cpp::RustCryptoProtocol> crypto_impl,
		rust::Box<cpp::RustStorageProtocol> storage_impl,
		uint32_t unlock_timeout_sec
	) {
		return std::make_unique<beekeeper_holder>(
			std::move(crypto_impl),
			std::move(storage_impl),
			unlock_timeout_sec
		);
	}

	std::unique_ptr<beekeeper_holder> new_beekeeper_holder_in_memory(
		rust::Box<cpp::RustCryptoProtocol> crypto_impl,
		uint32_t unlock_timeout_sec
	) {
		return std::make_unique<beekeeper_holder>(
			std::move(crypto_impl),
			unlock_timeout_sec
		);
	}
}
