# Minimal WASM build configuration — no FC, no Boost, no OpenSSL.
# All crypto primitives are provided by JS callbacks.
# Single universal target for web, worker, and Node.js environments.

set(WASM_RUNTIME_COMPONENT_NAME "wasm_runtime_components")

function( DEFINE_WASM_TARGET wasm_target_basename )
  set(options)
  set(oneValueArgs)
  set(multiValueArgs LINK_LIBRARIES LINK_OPTIONS)
  cmake_parse_arguments(PARSE_ARGV 0 arg
    "${options}" "${oneValueArgs}" "${multiValueArgs}"
  )

  set( exec_common_name "${wasm_target_basename}.common" )

  message(NOTICE "Configuring '${exec_common_name}'")

  ADD_EXECUTABLE( ${exec_common_name} ${SOURCES} )

  target_include_directories( ${exec_common_name} PUBLIC ${INCLUDES} )

  target_compile_options( ${exec_common_name} PUBLIC
    -Oz
  )

  target_link_libraries( ${exec_common_name} PUBLIC embind )

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
    # crypto_provider_impl — calls async primitives (some inlined by -Oz)
    "*crypto_provider_impl::encrypt_wallet_keys*"
    "*crypto_provider_impl::encrypt_wallet_data*"
    "*crypto_provider_impl::decrypt_wallet_data*"
    "*crypto_provider_impl::validate_password*"
    "*crypto_provider_impl::ecdh_encrypt*"
    "*crypto_provider_impl::ecdh_decrypt*"
    "*crypto_provider_impl::wif_to_key*"
    "*crypto_provider_impl::key_to_wif*"
    # wallet — encrypt_and_save calls async crypto, but lock() is now sync (no longer saves)
    "*wallet::encrypt_and_save*"
    # beekeeper_api — embind entry points (only those that transitively call async crypto)
    "*beekeeper_api::create*"
    "*beekeeper_api::unlock*"
    "*beekeeper_api::import_key*"
    "*beekeeper_api::remove_key*"
    "*beekeeper_api::sign_digest*"
    "*beekeeper_api::encrypt_data*"
    "*beekeeper_api::decrypt_data*"
    # Embind dispatch layer (Invoker call beekeeper_api methods)
    "*Invoker*"
    # emscripten::val — await, call, and internalCall are in the async path
    "*emscripten::val*"
    # wasm_crypto_primitives helpers called around val::await
    "*wasm_crypto_primitives::to_js*"
    # Emscripten dynCall trampolines (exception-safe indirect calls)
    "*dynCall*"
  )
  list(JOIN ASYNCIFY_FUNCS "','" ASYNCIFY_FUNCS_STR)

  target_link_options( ${exec_common_name} PUBLIC
    -Oz
    -sDISABLE_EXCEPTION_CATCHING=0
    -sEXPORT_EXCEPTION_HANDLING_HELPERS=1
    -sEXCEPTION_STACK_TRACES=1
    -sMODULARIZE=1 -sSINGLE_FILE=0 -sUSE_ES6_IMPORT_META=1
    -sEXPORT_ES6=1 -sINITIAL_MEMORY=67108864 -sWASM_ASYNC_COMPILATION=1
    -sNO_FILESYSTEM=1
    -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=65536
    "-sASYNCIFY_ONLY=['${ASYNCIFY_FUNCS_STR}']"
    --minify=0 --emit-symbol-map -sENVIRONMENT=web,worker,node
    --emit-tsd "${CMAKE_CURRENT_BINARY_DIR}/${exec_common_name}.d.ts"
  )

  if (arg_LINK_OPTIONS)
    target_link_options( ${exec_common_name} PUBLIC ${arg_LINK_OPTIONS})
  endif()

  if (arg_LINK_LIBRARIES)
    target_link_libraries( ${exec_common_name} PUBLIC ${arg_LINK_LIBRARIES})
  endif()

  set_target_properties( ${exec_common_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    OUTPUT_NAME "${exec_common_name}.js"
  )

  INSTALL( FILES "${CMAKE_BINARY_DIR}/${exec_common_name}.js"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )

  INSTALL( FILES "${CMAKE_BINARY_DIR}/${exec_common_name}.wasm"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )

  INSTALL( FILES  "${CMAKE_CURRENT_BINARY_DIR}/${exec_common_name}.d.ts"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )
endfunction()
