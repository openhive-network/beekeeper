# secp256k1-zkp for Emscripten — compile secp256k1.c directly (no autotools).
# Mirrors the MSVC block in fc/CMakeLists.txt with modules we need.

set(SECP256K1_ZKP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../libraries/plugins/libraries/fc/vendor/secp256k1-zkp")

add_library(secp256k1_wasm STATIC
    "${SECP256K1_ZKP_DIR}/src/secp256k1.c"
    "${SECP256K1_ZKP_DIR}/src/precomputed_ecmult.c"
    "${SECP256K1_ZKP_DIR}/src/precomputed_ecmult_gen.c"
)

target_include_directories(secp256k1_wasm
    PRIVATE "${SECP256K1_ZKP_DIR}"
    PRIVATE "${SECP256K1_ZKP_DIR}/src"
    PUBLIC  "${SECP256K1_ZKP_DIR}/include"
)

target_compile_definitions(secp256k1_wasm PRIVATE
    USE_FIELD_10X26        # 32-bit field (WASM is 32-bit, no 64-bit int multiply)
    USE_FIELD_INV_BUILTIN
    USE_NUM_NONE
    USE_SCALAR_8X32        # 32-bit scalar
    USE_SCALAR_INV_BUILTIN
    ENABLE_MODULE_RECOVERY # needed for secp256k1_ecdsa_sign_recoverable
    SECP256K1_BUILD
    ECMULT_WINDOW_SIZE=2   # minimal tables for small WASM binary
    ECMULT_GEN_PREC_BITS=2
    USE_EXTERNAL_DEFAULT_CALLBACKS # avoid fprintf in default callbacks — saves ~3 KB WASM
)

target_compile_options(secp256k1_wasm PRIVATE -Oz)
set_target_properties(secp256k1_wasm PROPERTIES LINKER_LANGUAGE C)
