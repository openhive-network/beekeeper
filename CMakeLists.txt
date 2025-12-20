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

# Add plugins submodule (includes fc and appbase)
add_subdirectory(libraries/plugins)

# Create target for minimal protocol config (standalone beekeeper builds only)
add_library(beekeeper_protocol_config INTERFACE)
target_include_directories(beekeeper_protocol_config INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Add beekeeper programs
add_subdirectory(programs/beekeeper)
