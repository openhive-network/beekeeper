# Beekeeper Unit Tests
#
# These tests are ported from the hive repository and test the beekeeper
# wallet manager functionality.
#
# Some tests require HIVE_PROTOCOL_AVAILABLE to be defined (for transaction signing tests)
# Some tests require HIVE_CHAIN_AVAILABLE to be defined (for database fixture tests)
#
# When building standalone beekeeper without hive protocol, these protocol-dependent
# tests are automatically skipped via preprocessor guards.

cmake_minimum_required(VERSION 3.16)

find_package(Boost REQUIRED COMPONENTS unit_test_framework)

# Main test executable
add_executable(beekeeper_test
    beekeeper_tests.cpp
    beekeeper_api_tests.cpp
)

target_include_directories(beekeeper_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/programs/beekeeper/core/include
    ${CMAKE_SOURCE_DIR}/programs/beekeeper/beekeeper/include
)

target_link_libraries(beekeeper_test PRIVATE
    beekeeper_core
    beekeeper_lib
    fc
    appbase
    Boost::unit_test_framework
)

# Define IS_TEST_NET to enable test compilation
target_compile_definitions(beekeeper_test PRIVATE IS_TEST_NET)

# Optional: Define HIVE_PROTOCOL_AVAILABLE if hive protocol is linked
# target_compile_definitions(beekeeper_test PRIVATE HIVE_PROTOCOL_AVAILABLE)

# Optional: Define HIVE_CHAIN_AVAILABLE if full hive chain is available
# target_compile_definitions(beekeeper_test PRIVATE HIVE_CHAIN_AVAILABLE)

# Register with CTest
add_test(NAME beekeeper_unit_tests COMMAND beekeeper_test --log_level=test_suite)

# ── core vs core_minimal comparison tests ──
add_executable(core_comparison_test
    core_comparison_tests.cpp
)

target_include_directories(core_comparison_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/programs/beekeeper/core/include
    ${CMAKE_SOURCE_DIR}/programs/beekeeper/core_minimal/include
    ${CMAKE_SOURCE_DIR}/programs/beekeeper/fc_crypto_bridge/include
)

target_link_libraries(core_comparison_test PRIVATE
    beekeeper_core
    beekeeper_core_minimal
    fc_crypto_bridge
    fc
    Boost::unit_test_framework
)

add_test(NAME core_comparison_tests COMMAND core_comparison_test --log_level=test_suite)
