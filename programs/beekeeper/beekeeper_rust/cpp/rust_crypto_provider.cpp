#include <beekeeper_rs/rust_crypto_provider.hpp>

#include <stdexcept>

namespace beekeeper_rs {
	namespace {
		[[noreturn]] void not_implemented(const char* op) {
			throw std::logic_error(
				std::string("rust_crypto_provider: ")
				+ op
				+ " not implemented");
		}
	}

	rust_crypto_provider::rust_crypto_provider(
		rust::Box<cpp::RustCryptoProtocol> impl
	) : impl_(std::move(impl)) {}

	rust_crypto_provider::~rust_crypto_provider() = default;

	beekeeper_minimal::private_key_type
	rust_crypto_provider::generate_private_key() {
		not_implemented("generate_private_key");
	}

	std::optional<beekeeper_minimal::private_key_type>
	rust_crypto_provider::wif_to_key(const std::string&) {
		not_implemented("wif_to_key");
	}

	std::string rust_crypto_provider::key_to_wif(
		const beekeeper_minimal::private_key_type&
	) {
		not_implemented("key_to_wif");
	}

	beekeeper_minimal::public_key_type rust_crypto_provider::get_public_key(
		const beekeeper_minimal::private_key_type&
	) {
		not_implemented("get_public_key");
	}

	std::string rust_crypto_provider::public_key_to_string(
		const beekeeper_minimal::public_key_type&,
		const std::string&
	) {
		not_implemented("public_key_to_string");
	}

	beekeeper_minimal::public_key_type
	rust_crypto_provider::public_key_from_string(
		const std::string&,
		const std::string&
	) {
		not_implemented("public_key_from_string");
	}

	beekeeper_minimal::signature_type
	rust_crypto_provider::sign_compact(
		const beekeeper_minimal::private_key_type&,
		const beekeeper_minimal::digest_type&
	) {
		not_implemented("sign_compact");
	}

	beekeeper_minimal::digest_type
	rust_crypto_provider::digest_from_hex(
		const std::string&
	) {
		not_implemented("digest_from_hex");
	}

	std::string
	rust_crypto_provider::signature_to_hex(
		const beekeeper_minimal::signature_type&
	) {
		not_implemented("signature_to_hex");
	}

	std::vector<char> rust_crypto_provider::encrypt_wallet_keys(
		const std::string&,
		const beekeeper_minimal::keys_map&
	) {
		not_implemented("encrypt_wallet_keys");
	}

	std::vector<char> rust_crypto_provider::encrypt_wallet_data(
		const std::string&,
		const beekeeper_minimal::keys_map&
	) {
		not_implemented("encrypt_wallet_data");
	}

	beekeeper_minimal::keys_map rust_crypto_provider::decrypt_wallet_data(
		const std::string&,
		const std::vector<char>&
	) {
		not_implemented("decrypt_wallet_data");
	}

	std::vector<char> rust_crypto_provider::parse_wallet_file(
		const std::vector<char>&
	) {
		not_implemented("parse_wallet_file");
	}

	void rust_crypto_provider::validate_password(
		const std::string&,
		const std::vector<char>&
	) {
		not_implemented("validate_password");
	}

	std::string rust_crypto_provider::ecdh_encrypt(
		const beekeeper_minimal::private_key_type&,
		const beekeeper_minimal::public_key_type&,
		const std::string&,
		std::optional<uint64_t>
	) {
		not_implemented("ecdh_encrypt");
	}

	void rust_crypto_provider::get_random_bytes(
		uint8_t*,
		size_t
	) {
		not_implemented("get_random_bytes");
	}

	std::string rust_crypto_provider::ecdh_decrypt(
		const beekeeper_minimal::private_key_type&,
		const beekeeper_minimal::public_key_type&,
		const std::string&
	) {
		not_implemented("ecdh_decrypt");
	}
}
