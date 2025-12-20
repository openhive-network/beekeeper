# Beekeeper - Standalone wallet daemon for Hive blockchain
#
# This project uses hive as a submodule. To build:
#
#   mkdir build && cd build
#   cmake ../hive -DCMAKE_BUILD_TYPE=Release
#   make -j$(nproc) beekeeper
#
# This CMakeLists.txt is provided for IDE integration and documentation.
# The actual build uses hive's CMakeLists.txt directly.
#
cmake_minimum_required(VERSION 3.16)
project(Beekeeper VERSION 1.0.0 DESCRIPTION "Standalone wallet daemon for Hive blockchain")

message(STATUS "")
message(STATUS "=== Beekeeper Standalone Build ===")
message(STATUS "")
message(STATUS "To build beekeeper, run cmake on the hive subdirectory:")
message(STATUS "")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake ../hive -DCMAKE_BUILD_TYPE=Release -GNinja")
message(STATUS "  ninja beekeeper")
message(STATUS "")
message(STATUS "The beekeeper binary will be at: build/programs/beekeeper/beekeeper")
message(STATUS "")
