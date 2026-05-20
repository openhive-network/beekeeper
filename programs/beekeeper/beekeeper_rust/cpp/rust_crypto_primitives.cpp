#include <beekeeper_rs/rust_crypto_primitives.hpp>

#include "beekeeper_rust/src/lib.rs.h"

#include <cstring>

namespace beekeeper_rs {
	rust_crypto_primitives::rust_crypto_primitives(
		rust::Box<cpp::RustCryptoProtocol> impl
	) : impl_(std::move(impl)) {}

	rust_crypto_primitives::~rust_crypto_primitives() = default;

	beekeeper_minimal::digest_type
	rust_crypto_primitives::sha256(const uint8_t* data, size_t len) {
		beekeeper_minimal::digest_type out;
		impl_->cpp_sha256(
			rust::Slice<const uint8_t>{data, len},
			rust::Slice<uint8_t>{out.begin(), out.size()}
		);
		return out;
	}

	beekeeper_minimal::sha512_hash
	rust_crypto_primitives::sha512(const uint8_t* data, size_t len) {
		beekeeper_minimal::sha512_hash out;
		impl_->cpp_sha512(
			rust::Slice<const uint8_t>{data, len},
			rust::Slice<uint8_t>{out.begin(), out.size()}
		);
		return out;
	}

	std::array<uint8_t, 20>
	rust_crypto_primitives::ripemd160(const uint8_t* data, size_t len) {
		std::array<uint8_t, 20> out{};
		impl_->cpp_ripemd160(
			rust::Slice<const uint8_t>{data, len},
			rust::Slice<uint8_t>{out.data(), out.size()}
		);
		return out;
	}

	std::vector<uint8_t>
	rust_crypto_primitives::aes256_cbc_encrypt(
		const uint8_t* key, const uint8_t* iv,
		const uint8_t* data, size_t len
	) {
		auto vec = impl_->cpp_aes256_cbc_encrypt(
			rust::Slice<const uint8_t>{key, 32},
			rust::Slice<const uint8_t>{iv, 16},
			rust::Slice<const uint8_t>{data, len}
		);
		return std::vector<uint8_t>(vec.begin(), vec.end());
	}

	std::vector<uint8_t>
	rust_crypto_primitives::aes256_cbc_decrypt(
		const uint8_t* key, const uint8_t* iv,
		const uint8_t* data, size_t len
	) {
		auto vec = impl_->cpp_aes256_cbc_decrypt(
			rust::Slice<const uint8_t>{key, 32},
			rust::Slice<const uint8_t>{iv, 16},
			rust::Slice<const uint8_t>{data, len}
		);
		return std::vector<uint8_t>(vec.begin(), vec.end());
	}

	beekeeper_minimal::private_key_type
	rust_crypto_primitives::generate_private_key() {
		beekeeper_minimal::private_key_type out;
		impl_->cpp_generate_private_key(
			rust::Slice<uint8_t>{out.begin(), out.size()}
		);
		return out;
	}

	beekeeper_minimal::public_key_type
	rust_crypto_primitives::get_public_key(
		const beekeeper_minimal::private_key_type& privkey
	) {
		beekeeper_minimal::public_key_type out;
		impl_->cpp_get_public_key(
			rust::Slice<const uint8_t>{privkey.begin(), privkey.size()},
			rust::Slice<uint8_t>{out.begin(), out.size()}
		);
		return out;
	}

	beekeeper_minimal::signature_type
	rust_crypto_primitives::sign_compact(
		const beekeeper_minimal::private_key_type& privkey,
		const beekeeper_minimal::digest_type& digest
	) {
		beekeeper_minimal::signature_type out;
		impl_->cpp_sign_compact(
			rust::Slice<const uint8_t>{privkey.begin(), privkey.size()},
			rust::Slice<const uint8_t>{digest.begin(), digest.size()},
			rust::Slice<uint8_t>{out.begin(), out.size()}
		);
		return out;
	}

	beekeeper_minimal::sha512_hash
	rust_crypto_primitives::ecdh_shared_secret(
		const beekeeper_minimal::private_key_type& privkey,
		const beekeeper_minimal::public_key_type& pubkey
	) {
		beekeeper_minimal::sha512_hash out;
		impl_->cpp_ecdh_shared_secret(
			rust::Slice<const uint8_t>{privkey.begin(), privkey.size()},
			rust::Slice<const uint8_t>{pubkey.begin(), pubkey.size()},
			rust::Slice<uint8_t>{out.begin(), out.size()}
		);
		return out;
	}

	size_t rust_crypto_primitives::base58_encode(
		const uint8_t* data, size_t data_len,
		char* out, size_t out_size
	) {
		auto s = impl_->cpp_base58_encode(
			rust::Slice<const uint8_t>{data, data_len}
		);
		if (s.size() + 1 > out_size) return 0;
		std::memcpy(out, s.data(), s.size());
		out[s.size()] = '\0';
		return s.size();
	}

	size_t rust_crypto_primitives::base58_decode(
		const char* str, size_t str_len,
		uint8_t* out, size_t out_size
	) {
		try {
			auto vec = impl_->cpp_base58_decode(rust::Str{str, str_len});
			if (vec.size() > out_size) return 0;
			std::memcpy(out, vec.data(), vec.size());
			return vec.size();
		} catch (...) {
			return 0;
		}
	}

	void rust_crypto_primitives::get_random_bytes(uint8_t* buf, size_t len) {
		impl_->cpp_get_random_bytes(rust::Slice<uint8_t>{buf, len});
	}
}
