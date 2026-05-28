# Minimal CMake driver that builds only the fc_crypto_bridge static library and
# its deps (fc, secp256k1, beekeeper_core_minimal) for the beekeeper_rust crate.
#
# The beekeeper top-level CMakeLists pulls in the whole daemon (appbase,
# webserver, …), which requires Boost components the CI image does not ship
# (program_options, date_time, iostreams, …). This driver builds only what
# beekeeper_rust links against, keeping the crate self-contained so it can be
# consumed like a normal cargo dependency.
cmake_minimum_required(VERSION 3.16)
project(beekeeper_fc_bridge C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# …/crates/beekeeper — four directories above this file.
get_filename_component(BEEKEEPER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(FC_ROOT "${BEEKEEPER_ROOT}/libraries/plugins/libraries/fc")

list(APPEND CMAKE_MODULE_PATH
  "${FC_ROOT}/CMakeModules"
  "${FC_ROOT}/GitVersionGen")

# Pin Boost to the components present in the CI base image. fc's own default
# lists request date_time/iostreams/locale/regex, which the image does not ship;
# setting BOOST_COMPONENTS up front makes fc skip those (`if(NOT BOOST_COMPONENTS)`).
set(BOOST_COMPONENTS chrono context coroutine filesystem system thread unit_test_framework)

# Force legacy FindBoost (config mode rejects the image's static-runtime .a libs)
# and prefer static libs. hive_targets caches these too, but only once fc's main
# CMakeLists runs the macro — fc/tests/CMakeLists does its own find_package first,
# so the values must already be cached before fc is added. (In the full hive build
# an earlier sibling target seeds the cache; here fc is the first subdirectory.)
set(Boost_NO_BOOST_CMAKE ON CACHE BOOL "" FORCE)
set(Boost_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)

if(DEFINED ENV{OPENSSL_ROOT_DIR} AND NOT DEFINED CACHE{OPENSSL_ROOT_DIR})
  set(OPENSSL_ROOT_DIR "$ENV{OPENSSL_ROOT_DIR}" CACHE PATH "OpenSSL root directory")
endif()
if(DEFINED ENV{OPENSSL_INCLUDE_DIR} AND NOT DEFINED CACHE{OPENSSL_INCLUDE_DIR})
  set(OPENSSL_INCLUDE_DIR "$ENV{OPENSSL_INCLUDE_DIR}" CACHE PATH "OpenSSL include directory")
endif()

add_subdirectory("${FC_ROOT}"
                 "${CMAKE_CURRENT_BINARY_DIR}/fc")
add_subdirectory("${BEEKEEPER_ROOT}/programs/beekeeper/core_minimal"
                 "${CMAKE_CURRENT_BINARY_DIR}/core_minimal")
add_subdirectory("${BEEKEEPER_ROOT}/programs/beekeeper/fc_crypto_bridge"
                 "${CMAKE_CURRENT_BINARY_DIR}/fc_crypto_bridge")
