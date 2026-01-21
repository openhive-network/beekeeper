cmake_minimum_required(VERSION 3.22.1)
project(BeekeeperWASMMinimal)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use fc library components (reusing battle-tested implementations)
set(FC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../libraries/plugins/libraries/fc")

# Use the secp256k1 library from the fc submodule
set(SECP256K1_DIR "${FC_DIR}/vendor/secp256k1-zkp")

# Build secp256k1 as a static library
# Configure for minimal WASM build - no benchmarks, no tests
set(SECP256K1_ENABLE_MODULE_RECOVERY ON CACHE BOOL "Enable recovery module" FORCE)
set(SECP256K1_BUILD_BENCHMARK OFF CACHE BOOL "Disable benchmarks" FORCE)
set(SECP256K1_BUILD_TESTS OFF CACHE BOOL "Disable tests" FORCE)
set(SECP256K1_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples" FORCE)

# For Emscripten, we need to compile secp256k1 directly
# Include precomputed tables for ecmult (required for linking)
# Note: These tables add ~1MB to WASM but are needed for signing
set(SECP256K1_SOURCES
    "${SECP256K1_DIR}/src/secp256k1.c"
    "${SECP256K1_DIR}/src/precomputed_ecmult.c"
    "${SECP256K1_DIR}/src/precomputed_ecmult_gen.c"
)

add_library(secp256k1_minimal STATIC ${SECP256K1_SOURCES})
target_include_directories(secp256k1_minimal PUBLIC "${SECP256K1_DIR}/include" "${SECP256K1_DIR}")

# secp256k1 configuration for WASM (no assembly, portable C)
# Using smaller precomputed tables for smaller WASM size
# Trade-off: slower computation but much smaller binary
target_compile_definitions(secp256k1_minimal PRIVATE
    USE_FIELD_10X26=1
    USE_FIELD_INV_BUILTIN=1
    USE_NUM_NONE=1
    USE_SCALAR_8X32=1
    USE_SCALAR_INV_BUILTIN=1
    USE_ECMULT_STATIC_PRECOMPUTATION=1
    ECMULT_WINDOW_SIZE=2        # Smaller window = smaller tables (default 15)
    ECMULT_GEN_PREC_BITS=2      # Smaller precision = smaller tables (default 4)
    ENABLE_MODULE_RECOVERY=1
)

# Enable size optimization for secp256k1
target_compile_options(secp256k1_minimal PRIVATE -Oz -fno-exceptions)

# FC crypto sources (standalone, no OpenSSL dependencies)
# CRIPEMD160: Bitcoin Core's RIPEMD-160 implementation (battle-tested)
set(FC_CRYPTO_SOURCES
    "${FC_DIR}/src/crypto/cripemd160.cpp"
)

# Main WASM module
add_executable(beekeeper_wasm_minimal wasm_api.cpp ${FC_CRYPTO_SOURCES})

target_include_directories(beekeeper_wasm_minimal PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${SECP256K1_DIR}/include"
    "${FC_DIR}/include"  # For fc/crypto/cripemd160.hpp
)

target_link_libraries(beekeeper_wasm_minimal PRIVATE secp256k1_minimal embind)

# Emscripten-specific flags for minimal output
# Note: embind needs RTTI for type names, so we can't use -fno-rtti
target_compile_options(beekeeper_wasm_minimal PRIVATE
    -Oz                     # Optimize for size
    -fno-exceptions         # No C++ exceptions (use return codes)
)

target_link_options(beekeeper_wasm_minimal PRIVATE
    -Oz
    -sMODULARIZE=1
    -sSINGLE_FILE=0
    -sEXPORT_ES6=1
    -sUSE_ES6_IMPORT_META=1
    -sENVIRONMENT=web,node
    -sWASM_ASYNC_COMPILATION=1
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=16777216  # 16MB initial
    -sSTACK_SIZE=65536         # 64KB stack
    --no-entry
    --minify=0
)

# Output naming
set_target_properties(beekeeper_wasm_minimal PROPERTIES
    OUTPUT_NAME "beekeeper_minimal"
    SUFFIX ".js"
)

# Installation
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/beekeeper_minimal.js"
    "${CMAKE_CURRENT_BINARY_DIR}/beekeeper_minimal.wasm"
    DESTINATION .
)
