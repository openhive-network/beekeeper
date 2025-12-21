cmake_minimum_required(VERSION 3.16)
project(Beekeeper VERSION 1.0.0 DESCRIPTION "Standalone wallet daemon for Hive blockchain")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "")
message(STATUS "=== Beekeeper Standalone Build ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "")

# Find Boost with unit_test_framework for fc tests
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

# Disable websocketpp tests/examples to avoid Boost CMake compatibility issues
# (websocketpp's Boost detection is incompatible with newer Boost/CMake versions)
set(BUILD_TESTS OFF CACHE BOOL "Disable websocketpp tests" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable websocketpp examples" FORCE)

# Add plugins submodule (includes fc and appbase)
add_subdirectory(libraries/plugins)

# Re-enable tests for beekeeper itself
set(BUILD_TESTS ON CACHE BOOL "Enable beekeeper tests" FORCE)

# Fix fc tests linking - they need Boost.Unit_Test_Framework
# (fc tests link only to fc, but don't get Boost.Test symbols)
foreach(fc_test_target all_tests bloom_test ntp_test task_cancel_test thread_test
                       real128_test blind hmac_test ecc_test log_test
                       saturation_test ecdsa_canon_test sha_test)
  if(TARGET ${fc_test_target})
    target_link_libraries(${fc_test_target} Boost::unit_test_framework)
  endif()
endforeach()

# Create target for minimal protocol config (standalone beekeeper builds only)
add_library(beekeeper_protocol_config INTERFACE)
target_include_directories(beekeeper_protocol_config INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Add beekeeper programs
add_subdirectory(programs/beekeeper)

# Optional: Build tests
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
  message(STATUS "Building tests enabled")

  # Build native beekeeper_wasm library for testing
  # (The actual beekeeper_wasm is WASM-only, but tests need a native version)
  add_library(beekeeper_wasm_native STATIC
    programs/beekeeper/beekeeper_wasm/beekeeper_wasm_api.cpp
    programs/beekeeper/beekeeper_wasm/beekeeper_wasm_app.cpp
  )
  target_include_directories(beekeeper_wasm_native PUBLIC
    programs/beekeeper/beekeeper_wasm/include
  )
  target_link_libraries(beekeeper_wasm_native PUBLIC
    beekeeper_core
  )

  add_subdirectory(tests)
endif()
