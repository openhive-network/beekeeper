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

		beekeeper_minimal::private_key_type generate_private_key() override;
		std::optional<beekeeper_minimal::private_key_type> wif_to_key(
			const std::string& wif
		) override;
		std::string key_to_wif(
			const beekeeper_minimal::private_key_type& key
		) override;
		beekeeper_minimal::public_key_type get_public_key(
			const beekeeper_minimal::private_key_type& key
		) override;
		std::string public_key_to_string(
			const beekeeper_minimal::public_key_type& key,
			const std::string& prefix
		) override;
		beekeeper_minimal::public_key_type public_key_from_string(
			const std::string& str,
			const std::string& prefix
		) override;
		beekeeper_minimal::signature_type sign_compact(
			const beekeeper_minimal::private_key_type& key,
			const beekeeper_minimal::digest_type& digest
		) override;
		beekeeper_minimal::digest_type digest_from_hex(
			const std::string& hex
		) override;
		std::string signature_to_hex(
			const beekeeper_minimal::signature_type& sig
		) override;
		std::vector<char> encrypt_wallet_keys(
			const std::string& password,
			const beekeeper_minimal::keys_map& keys
		) override;
		std::vector<char> encrypt_wallet_data(
			const std::string& password,
			const beekeeper_minimal::keys_map& keys
		) override;
		beekeeper_minimal::keys_map decrypt_wallet_data(
			const std::string& password,
			const std::vector<char>& cipher_keys
		) override;
		std::vector<char> parse_wallet_file(
			const std::vector<char>& wallet_file_content
		) override;
		void validate_password(
			const std::string& password,
			const std::vector<char>& cipher_keys
		) override;
		std::string ecdh_encrypt(
			const beekeeper_minimal::private_key_type& from_key,
			const beekeeper_minimal::public_key_type& to_key,
			const std::string& content,
			std::optional<uint64_t> nonce = {}
		) override;
		void get_random_bytes(
			uint8_t* buf,
			size_t len
		) override;
		std::string ecdh_decrypt(
			const beekeeper_minimal::private_key_type& priv_key,
			const beekeeper_minimal::public_key_type& other_pub,
			const std::string& encrypted_content
		) override;

	private:
		rust::Box<cpp::RustCryptoProtocol> impl_;
	};
}
