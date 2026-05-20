#pragma once

#include <core_minimal/crypto_primitives.hpp>

#include <rust/cxx.h>

namespace cpp {
	struct RustCryptoProtocol;
}

namespace beekeeper_rs {
	/// Low-level crypto primitives implemented in Rust. The high-level
	/// crypto_provider methods (WIF, wallet JSON, ECDH formatting, etc.)
	/// stay in C++ crypto_provider_impl, which calls into this for raw ops.
	class rust_crypto_primitives final : public beekeeper_minimal::crypto_primitives
	{
	public:
		explicit rust_crypto_primitives(rust::Box<cpp::RustCryptoProtocol> impl);
		~rust_crypto_primitives() override;

		beekeeper_minimal::digest_type sha256(const uint8_t* data, size_t len) override;
		beekeeper_minimal::sha512_hash sha512(const uint8_t* data, size_t len) override;
		std::array<uint8_t, 20>        ripemd160(const uint8_t* data, size_t len) override;

		std::vector<uint8_t> aes256_cbc_encrypt(
			const uint8_t* key, const uint8_t* iv,
			const uint8_t* data, size_t len) override;
		std::vector<uint8_t> aes256_cbc_decrypt(
			const uint8_t* key, const uint8_t* iv,
			const uint8_t* data, size_t len) override;

		beekeeper_minimal::private_key_type generate_private_key() override;
		beekeeper_minimal::public_key_type  get_public_key(const beekeeper_minimal::private_key_type& privkey) override;
		beekeeper_minimal::signature_type   sign_compact(
			const beekeeper_minimal::private_key_type& privkey,
			const beekeeper_minimal::digest_type& digest) override;
		beekeeper_minimal::sha512_hash      ecdh_shared_secret(
			const beekeeper_minimal::private_key_type& privkey,
			const beekeeper_minimal::public_key_type& pubkey) override;

		size_t base58_encode(const uint8_t* data, size_t data_len,
		                     char* out, size_t out_size) override;
		size_t base58_decode(const char* str, size_t str_len,
		                     uint8_t* out, size_t out_size) override;

		void get_random_bytes(uint8_t* buf, size_t len) override;

	private:
		rust::Box<cpp::RustCryptoProtocol> impl_;
	};
}
