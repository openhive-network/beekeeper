# Minimal WASM build configuration — no FC, no Boost, no OpenSSL.
# All crypto primitives are provided by JS callbacks.

set(WASM_RUNTIME_COMPONENT_NAME "wasm_runtime_components")

function( DEFINE_WASM_TARGET_FOR wasm_target_basename )
  set(options WASM_USE_FS)
  set(oneValueArgs TARGET_ENVIRONMENT)
  set(multiValueArgs LINK_LIBRARIES LINK_OPTIONS)
  cmake_parse_arguments(PARSE_ARGV 0 arg
    "${options}" "${oneValueArgs}" "${multiValueArgs}"
  )

  set( exec_wasm_name "${wasm_target_basename}.${arg_TARGET_ENVIRONMENT}" )
  set( exec_common_name "${wasm_target_basename}.common" )

  message(NOTICE "Configuring '${exec_wasm_name}'")

  IF ("${arg_TARGET_ENVIRONMENT}" STREQUAL "web")
    MESSAGE( STATUS "Chosen web target environment")
    SET ( NODE_ENV 0 )
    SET ( WASM_ENV "web" )
  ELSE()
    MESSAGE( STATUS "Chosen node target environment")
    SET ( NODE_ENV 1 )
    SET ( WASM_ENV "node" )
  ENDIF()

  ADD_EXECUTABLE( ${exec_wasm_name} ${SOURCES} )

  target_include_directories( ${exec_wasm_name} PUBLIC ${INCLUDES} )

  target_compile_options( ${exec_wasm_name} PUBLIC
    -Oz
  )

  target_link_libraries( ${exec_wasm_name} PUBLIC embind )

  # Only instrument functions in the async call chain (val::await → embind entry points).
  # Wildcard '*' matches mangled prefixes/suffixes. Being over-inclusive is safe (minor
  # overhead); being under-inclusive causes runtime "unreachable" crashes.

  # Thanks to this list we save up to ~100KB of WASM binary size by not instrumenting ~760 non-async functions with Asyncify.

  # Later on we should consider adding a prefix to all async functions in C++ (e.g. "async_") to avoid having to maintain this list manually and risk missing some functions.

  set( ASYNCIFY_FUNCS
    # Async primitives (call val::await on JS Promises)
    "*wasm_crypto_primitives::sha256*"
    "*wasm_crypto_primitives::sha512*"
    "*wasm_crypto_primitives::aes256_cbc_encrypt*"
    "*wasm_crypto_primitives::aes256_cbc_decrypt*"
    "*wasm_crypto_primitives::ecdh_shared_secret*"
    # crypto_provider_impl — calls async primitives
    "*crypto_provider_impl::aes_encrypt*"
    "*crypto_provider_impl::aes_decrypt*"
    "*crypto_provider_impl::generate_encrypted_key*"
    "*crypto_provider_impl::encrypt_wallet_data*"
    "*crypto_provider_impl::decrypt_wallet_data*"
    "*crypto_provider_impl::validate_password*"
    "*crypto_provider_impl::ecdh_encrypt*"
    "*crypto_provider_impl::ecdh_decrypt*"
    "*crypto_provider_impl::wif_to_key*"
    "*crypto_provider_impl::key_to_wif*"
    # wallet — calls crypto_provider_impl
    "*wallet::create*"
    "*wallet::open*"
    "*wallet::unlock*"
    "*wallet::lock*"
    "*wallet::encrypt_and_save*"
    "*wallet::import_key*"
    "*wallet::remove_key*"
    "*wallet::check_password*"
    # session — calls wallet
    "*session::create_wallet*"
    "*session::open_wallet*"
    "*session::unlock*"
    "*session::lock*"
    "*session::lock_all*"
    "*session::check_timeout*"
    "*session::import_key*"
    "*session::remove_key*"
    "*session::encrypt_data*"
    "*session::decrypt_data*"
    "*session::gen_password*"
    # beekeeper — check_timeouts can trigger lock_all → encrypt_and_save
    "*beekeeper::check_timeouts*"
    "*beekeeper::create_session*"
    "*beekeeper::close_session*"
    # beekeeper_api — embind entry points (wrap() is inlined into each)
    "*beekeeper_api::create*"
    "*beekeeper_api::open*"
    "*beekeeper_api::close*"
    "*beekeeper_api::unlock*"
    "*beekeeper_api::lock*"
    "*beekeeper_api::lock_all*"
    "*beekeeper_api::import_key*"
    "*beekeeper_api::remove_key*"
    "*beekeeper_api::sign_digest*"
    "*beekeeper_api::encrypt_data*"
    "*beekeeper_api::decrypt_data*"
    "*beekeeper_api::get_info*"
    "*beekeeper_api::has_matching_private_key*"
    "*beekeeper_api::get_public_keys*"
    "*beekeeper_api::create_session*"
    "*beekeeper_api::close_session*"
    # JS callback storage — called during wallet save/load in async context
    "*js_callback_storage::save*"
    "*js_callback_storage::load*"
    # Embind dispatch layer (MethodInvoker/Invoker call beekeeper_api methods)
    "*MethodInvoker*beekeeper_api*"
    "*Invoker*beekeeper_api*"
    "*operator_new*beekeeper_api*"
    # emscripten::val — await, call, and internalCall are in the async path
    "*emscripten::val*"
    # wasm_crypto_primitives helpers called around val::await
    "*wasm_crypto_primitives::from_js*"
    "*wasm_crypto_primitives::to_js*"
    # std::function wrappers for key_finder lambdas in decrypt_data
    "*__func*session::decrypt_data*"
    "*__func*session::encrypt_data*"
    # Emscripten invoke/dynCall trampolines (exception-safe indirect calls)
    "*__invoke*"
    "*dynCall*"
  )
  list(JOIN ASYNCIFY_FUNCS "','" ASYNCIFY_FUNCS_STR)

  target_link_options( ${exec_wasm_name} PUBLIC
    -Oz
    -sDISABLE_EXCEPTION_CATCHING=0
    -sEXPORT_EXCEPTION_HANDLING_HELPERS=1
    -sEXCEPTION_STACK_TRACES=1
    -sELIMINATE_DUPLICATE_FUNCTIONS=1
    -sMODULARIZE=1 -sSINGLE_FILE=0 -sUSE_ES6_IMPORT_META=1
    -sEXPORT_ES6=1 -sINITIAL_MEMORY=67108864 -sWASM_ASYNC_COMPILATION=1
    -sNO_FILESYSTEM=1
    -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=65536
    "-sASYNCIFY_ONLY=['${ASYNCIFY_FUNCS_STR}']"
    --minify=0 --emit-symbol-map -sENVIRONMENT=${WASM_ENV}
    --emit-tsd "${CMAKE_CURRENT_BINARY_DIR}/${exec_common_name}.d.ts"
  )

  if (arg_LINK_OPTIONS)
    target_link_options( ${exec_wasm_name} PUBLIC ${arg_LINK_OPTIONS})
  endif()

  if (arg_LINK_LIBRARIES)
    target_link_libraries( ${exec_wasm_name} PUBLIC ${arg_LINK_LIBRARIES})
  endif()

  set_target_properties( ${exec_wasm_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${arg_TARGET_ENVIRONMENT}"
    OUTPUT_NAME "${exec_common_name}.js"
  )

  INSTALL( FILES "${CMAKE_BINARY_DIR}/${arg_TARGET_ENVIRONMENT}/${exec_common_name}.js"
    RENAME "${exec_wasm_name}.js"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )

  INSTALL( FILES "${CMAKE_BINARY_DIR}/${arg_TARGET_ENVIRONMENT}/${exec_common_name}.wasm"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )

  INSTALL( FILES  "${CMAKE_CURRENT_BINARY_DIR}/${exec_common_name}.d.ts"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )
endfunction()
