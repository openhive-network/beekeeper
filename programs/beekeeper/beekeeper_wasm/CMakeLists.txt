PROJECT( BeekeeperWASM )
cmake_minimum_required( VERSION 3.22.1 )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_EXTENSIONS OFF )
set( CMAKE_CXX_STANDARD_REQUIRED ON )

set( CMAKE_C_STANDARD 99 )
set( CMAKE_C_STANDARD_REQUIRED ON )
set( CMAKE_EXECUTABLE_SUFFIX "" )

include( ${CMAKE_CURRENT_SOURCE_DIR}/cmake/wasm_project.cmake )

add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../core_minimal beekeeper_core_minimal )
add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/../fc_crypto_bridge fc_crypto_bridge )

file(GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/beekeeper_wasm/*.hpp")

set( SOURCES
      main.cpp
      beekeeper_wasm_api.cpp
)

set( INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/include )

DEFINE_WASM_TARGET_FOR( beekeeper_wasm
      TARGET_ENVIRONMENT "web"
      LINK_LIBRARIES beekeeper_core_minimal fc_crypto_bridge
)

DEFINE_WASM_TARGET_FOR( beekeeper_wasm
      TARGET_ENVIRONMENT "node"
      LINK_LIBRARIES beekeeper_core_minimal fc_crypto_bridge
)
