#include <beekeeper_rs/rust_crypto_primitives.hpp>

#include <stdexcept>
#include <string>

namespace beekeeper_rs {
	namespace {
		[[noreturn]] void not_implemented(const char* op) {
			throw std::logic_error(
				std::string("rust_crypto_primitives: ")
				+ op
				+ " not implemented");
		}
	}

	rust_crypto_primitives::rust_crypto_primitives(
		rust::Box<cpp::RustCryptoProtocol> impl
	) : impl_(std::move(impl)) {}

	rust_crypto_primitives::~rust_crypto_primitives() = default;

	beekeeper_minimal::digest_type
	rust_crypto_primitives::sha256(const uint8_t*, size_t) {
		not_implemented("sha256");
	}

	beekeeper_minimal::sha512_hash
	rust_crypto_primitives::sha512(const uint8_t*, size_t) {
		not_implemented("sha512");
	}

	std::array<uint8_t, 20>
	rust_crypto_primitives::ripemd160(const uint8_t*, size_t) {
		not_implemented("ripemd160");
	}

	std::vector<uint8_t>
	rust_crypto_primitives::aes256_cbc_encrypt(
		const uint8_t*, const uint8_t*,
		const uint8_t*, size_t
	) {
		not_implemented("aes256_cbc_encrypt");
	}

	std::vector<uint8_t>
	rust_crypto_primitives::aes256_cbc_decrypt(
		const uint8_t*, const uint8_t*,
		const uint8_t*, size_t
	) {
		not_implemented("aes256_cbc_decrypt");
	}

	beekeeper_minimal::private_key_type
	rust_crypto_primitives::generate_private_key() {
		not_implemented("generate_private_key");
	}

	beekeeper_minimal::public_key_type
	rust_crypto_primitives::get_public_key(const beekeeper_minimal::private_key_type&) {
		not_implemented("get_public_key");
	}

	beekeeper_minimal::signature_type
	rust_crypto_primitives::sign_compact(
		const beekeeper_minimal::private_key_type&,
		const beekeeper_minimal::digest_type&
	) {
		not_implemented("sign_compact");
	}

	beekeeper_minimal::sha512_hash
	rust_crypto_primitives::ecdh_shared_secret(
		const beekeeper_minimal::private_key_type&,
		const beekeeper_minimal::public_key_type&
	) {
		not_implemented("ecdh_shared_secret");
	}

	size_t rust_crypto_primitives::base58_encode(
		const uint8_t*, size_t,
		char*, size_t
	) {
		not_implemented("base58_encode");
	}

	size_t rust_crypto_primitives::base58_decode(
		const char*, size_t,
		uint8_t*, size_t
	) {
		not_implemented("base58_decode");
	}

	void rust_crypto_primitives::get_random_bytes(uint8_t*, size_t) {
		not_implemented("get_random_bytes");
	}
}
