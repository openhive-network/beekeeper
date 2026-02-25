#include <beekeeper_wasm/beekeeper_wasm_api.hpp>

#include <emscripten/bind.h>

using namespace beekeeper_wasm;
using namespace emscripten;

EMSCRIPTEN_BINDINGS(beekeeper_api_instance) {
  class_<beekeeper_api>("beekeeper_api")

    /*
      ****creation of an instance of beekeeper****
      PARAMS:
      save_fn         : JS callback (name: string, data: Uint8Array) => void
      load_fn         : JS callback (name: string) => Uint8Array  (throws if not found)
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
      ****information about a session****
    */
    .function("get_info(token)", &beekeeper_api::get_info)

    /*
      ****testing if a private key corresponding to a public key exists in a wallet****
    */
    .function("has_matching_private_key(token, wallet_name, public_key)", &beekeeper_api::has_matching_private_key)
    ;
}

int main() {
  return 0;
}
