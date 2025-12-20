# Beekeeper - Standalone wallet daemon for Hive blockchain
#
# This is a wrapper project that builds only the beekeeper component from hive.
# The hive repository is included as a submodule.
#
cmake_minimum_required(VERSION 3.16)
project(Beekeeper VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set hive-specific options for minimal build
set(BUILD_HIVE_TESTNET OFF CACHE BOOL "Disable testnet build")
set(SKIP_BY_TX_ID ON CACHE BOOL "Skip by_tx_id index")

# Add hive subdirectory - it will build all required dependencies
add_subdirectory(hive)

# The beekeeper target is already defined in hive/programs/beekeeper
# We just expose it as the main target of this project

# Install beekeeper binary
install(TARGETS beekeeper
    RUNTIME DESTINATION bin
)

message(STATUS "Beekeeper standalone build configured")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
