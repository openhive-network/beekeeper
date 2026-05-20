#include <beekeeper_rs/beekeeper_holder.hpp>

#include <string>

namespace beekeeper_rs {
	beekeeper_holder::beekeeper_holder(
		rust::Box<cpp::RustCryptoProtocol> crypto_impl,
		rust::Box<cpp::RustStorageProtocol> storage_impl,
		uint32_t unlock_timeout_sec
	) : crypto_prims_(std::make_unique<rust_crypto_primitives>(std::move(crypto_impl))),
	    crypto_provider_(std::make_unique<beekeeper_minimal::crypto_provider_impl>(*crypto_prims_)),
	    storage_(std::make_unique<rust_wallet_storage>(std::move(storage_impl))),
	    bk_(std::make_unique<beekeeper_minimal::beekeeper>(*crypto_provider_, *storage_, unlock_timeout_sec))
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

	void beekeeper_holder::lock_all() {
		bk_->lock_all();
	}

	void beekeeper_holder::set_timeout(uint32_t seconds) {
		bk_->set_timeout(seconds);
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
}
