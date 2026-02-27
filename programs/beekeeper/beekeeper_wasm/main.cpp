#include <beekeeper_wasm/beekeeper_wasm_api.hpp>

#include <emscripten/bind.h>

using namespace beekeeper_wasm;
using namespace emscripten;

EMSCRIPTEN_BINDINGS(beekeeper_api_instance) {
  class_<beekeeper_api>("beekeeper_api")

    /*
      ****creation of an instance of beekeeper****
      PARAMS:
      storage         : JS object with storage callbacks:
                        save_fn(name: string, data: Uint8Array) => void,
                        load_fn(name: string) => Uint8Array  (throws if not found),
                        list_dir_fn() => string[]  (returns all stored wallet names)
      crypto          : JS object with crypto methods:
                        sha256(Uint8Array) => Promise<Uint8Array>,
                        sha512(Uint8Array) => Promise<Uint8Array>,
                        aes256CbcEncrypt(key, iv, data) => Promise<Uint8Array>,
                        aes256CbcDecrypt(key, iv, data) => Promise<Uint8Array>,
                        getRandomBytes(dest: Uint8Array) => void  (fills WASM view in place)
                        (secp256k1, ripemd160, base58 are native in WASM)
      unlock_timeout  : timeout for an unlocked wallet in seconds. Default: `900`

      RESULT:
        an instance of a beekeeper (ready to use, no separate init() call needed)
    */
    .constructor<val, val, uint32_t>()

    /*
      ****creation of a session****
      PARAMS:
        salt: a salt used for creation of a token. Not required.
              If the salt is:
                - not given, chosen is a version (1)
                -     given, chosen is a version (2)
      RESULT:
        {"token":"440c44f01dde9ef65e7b88c6d44f3a929bbf0ff993c06efa6d942d40b08567f3"}
        token: a token of a session created explicitly.
    */
    .function("create_session()", select_overload<std::string()>(&beekeeper_api::create_session))                       //(1)
    .function("create_session(salt)", select_overload<std::string(const std::string&)>(&beekeeper_api::create_session)) //(2)

    /*
      ****closing of a session****
    */
    .function("close_session(token)", &beekeeper_api::close_session)

    /*
      ****creation of a wallet****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet
        password:     a password used for creation of a wallet. Not required.
                      If the password is not given, chosen is a version (1), otherwise (2)
      RESULT:
        {"password":"PW5KNCWdnMZFKzrvVyA2xwKLRxcAZWxPoyGVSN9r38te3p1ceEjo1"}
    */
    .function("create(token, wallet_name)", select_overload<std::string(const std::string&, const std::string&)>(&beekeeper_api::create))                                             //(1)
    .function("create(token, wallet_name, password)", select_overload<std::string(const std::string&, const std::string&, const std::string&)>(&beekeeper_api::create))               //(2)
    .function("create(token, wallet_name, password, is_temporary)", select_overload<std::string(const std::string&, const std::string&, const std::string&, bool)>(&beekeeper_api::create)) //(3)

    /*
      ****unlocking of a wallet****
    */
    .function("unlock(token, wallet_name, password)", &beekeeper_api::unlock)

    /*
      ****opening of a wallet****
    */
    .function("open(token, wallet_name)", &beekeeper_api::open)

    /*
      ****closing of a wallet****
    */
    .function("close(token, wallet_name)", &beekeeper_api::close)

    /*
      ****locking all wallets****
    */
    .function("lock_all(token)", &beekeeper_api::lock_all)

    /*
      ****locking a wallet****
    */
    .function("lock(token, wallet_name)", &beekeeper_api::lock)

    /*
      ****importing of a private key into a wallet****
      RESULT:
        {"public_key":"STM6oR6ckA4TejTWTjatUdbcS98AKETc3rcnQ9dWxmeNiKDzfhBZa"}
    */
    .function("import_key(token, wallet_name, wif_key)", &beekeeper_api::import_key)

    /*
      ****removing of a private key from a wallet****
    */
    .function("remove_key(token, wallet_name, public_key)", &beekeeper_api::remove_key)

    /*
      ****listing of all public keys****
    */
    .function("get_public_keys(token)", select_overload<std::string(const std::string&)>(&beekeeper_api::get_public_keys))                                  //(1)
    .function("get_public_keys(token, wallet_name)", select_overload<std::string(const std::string&, const std::string&)>(&beekeeper_api::get_public_keys)) //(2)

    /*
      ****signing a transaction by signing a digest of the transaction****
    */
    .function("sign_digest(token, sig_digest, public_key)", select_overload<std::string(const std::string&, const std::string&, const std::string&)>(&beekeeper_api::sign_digest))              //(1)
    .function("sign_digest(token, sig_digest, public_key, wallet_name)", select_overload<std::string(const std::string&, const std::string&, const std::string&, const std::string&)>(&beekeeper_api::sign_digest)) //(2)

    /*
      ****encrypting data using ECDH****
      PARAMS:
        token:        a token representing a session
        wallet_name:  a name of wallet containing the private key for from_key
        from_key:     public key of the sender (private key must be in the wallet)
        to_key:       public key of the receiver
        content:      data to encrypt
        nonce:        optional nonce for deterministic encryption
      RESULT:
        {"encrypted_content":"<base58 encoded encrypted data>"}
    */
    .function("encrypt_data(token, wallet_name, from_key, to_key, content)", select_overload<std::string(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&)>(&beekeeper_api::encrypt_data))
    .function("encrypt_data(token, wallet_name, from_key, to_key, content, nonce)", select_overload<std::string(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&, uint32_t)>(&beekeeper_api::encrypt_data))

    /*
      ****decrypting data using ECDH****
      PARAMS:
        token:             a token representing a session
        wallet_name:       a name of wallet containing the private key
        from_key:          public key of the sender
        to_key:            public key of the receiver
        encrypted_content: base58 encoded encrypted data
      RESULT:
        {"decrypted_content":"<decrypted string>"}
    */
    .function("decrypt_data(token, wallet_name, from_key, to_key, encrypted_content)", &beekeeper_api::decrypt_data)

    /*
      ****information about a session****
    */
    .function("get_info(token)", &beekeeper_api::get_info)

    /*
      ****testing if a private key corresponding to a public key exists in a wallet****
    */
    .function("has_matching_private_key(token, wallet_name, public_key)", &beekeeper_api::has_matching_private_key)

    /*
      ****testing if a wallet with given name exists (in-memory or in storage)****
      RESULT:
        {"exists":true} or {"exists":false}
    */
    .function("has_wallet(token, wallet_name)", &beekeeper_api::has_wallet)

    /*
      ****listing all wallets (in-memory + storage)****
      RESULT:
        {"wallets":[{"name":"w0","unlocked":true},{"name":"w1","unlocked":false}]}
    */
    .function("list_wallets(token)", &beekeeper_api::list_wallets)
    ;
}

int main() {
  return 0;
}
