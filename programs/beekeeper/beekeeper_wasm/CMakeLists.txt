PROJECT( BeekeeperWASM )
cmake_minimum_required( VERSION 3.22.1 )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )
set( CMAKE_EXECUTABLE_SUFFIX "" )

include( ${CMAKE_CURRENT_SOURCE_DIR}/cmake/wasm_project.cmake )
include( ${CMAKE_CURRENT_SOURCE_DIR}/cmake/secp256k1_wasm.cmake )

add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../core_minimal beekeeper_core_minimal )

file(GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/beekeeper_wasm/*.hpp")

set( SOURCES
      main.cpp
      beekeeper_wasm_api.cpp
      wasm_crypto_primitives.cpp
      standalone_ripemd160.cpp
      standalone_base58.cpp
      cxa_demangle_stub.cpp
)

set( INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include )

DEFINE_WASM_TARGET_FOR( beekeeper_wasm
      TARGET_ENVIRONMENT "web"
      LINK_LIBRARIES beekeeper_core_minimal secp256k1_wasm
)

DEFINE_WASM_TARGET_FOR( beekeeper_wasm
      TARGET_ENVIRONMENT "node"
      LINK_LIBRARIES beekeeper_core_minimal secp256k1_wasm
)
