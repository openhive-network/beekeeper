find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(BEEKEEPER_RUST_PROFILE      $<IF:$<CONFIG:Debug>,debug,release>)
set(BEEKEEPER_RUST_TARGET_DIR   ${CMAKE_CURRENT_BINARY_DIR}/cargo)
set(BEEKEEPER_RUST_STATIC_LIB
    ${BEEKEEPER_RUST_TARGET_DIR}/${BEEKEEPER_RUST_PROFILE}/libbeekeeper_rust.a)

file(GLOB BEEKEEPER_RUST_SHIM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cpp/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/cpp/*.c"
)
file(GLOB BEEKEEPER_RUST_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/beekeeper_rs/*.hpp"
)
file(GLOB BEEKEEPER_RUST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.rs"
)

add_custom_command(
    OUTPUT  ${BEEKEEPER_RUST_STATIC_LIB}
    COMMAND ${CMAKE_COMMAND} -E env
              BEEKEEPER_CORE_MINIMAL_LIB_DIR=$<TARGET_FILE_DIR:beekeeper_core_minimal>
              BEEKEEPER_FC_CRYPTO_BRIDGE_LIB_DIR=$<TARGET_FILE_DIR:fc_crypto_bridge>
              BEEKEEPER_FC_CRYPTO_BRIDGE_INCLUDE_DIR=${CMAKE_SOURCE_DIR}/programs/beekeeper/fc_crypto_bridge/include
              ${CARGO_EXECUTABLE} build
                $<$<NOT:$<CONFIG:Debug>>:--release>
                --manifest-path ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
                --target-dir ${BEEKEEPER_RUST_TARGET_DIR}
    DEPENDS beekeeper_core_minimal
            fc_crypto_bridge
            ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml
            ${CMAKE_CURRENT_SOURCE_DIR}/Cargo.lock
            ${CMAKE_CURRENT_SOURCE_DIR}/build.rs
            ${BEEKEEPER_RUST_SOURCES}
            ${BEEKEEPER_RUST_SHIM_SOURCES}
            ${BEEKEEPER_RUST_HEADERS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building beekeeper_rust (cargo)"
    VERBATIM
)

add_custom_target(beekeeper_rust_build
    DEPENDS ${BEEKEEPER_RUST_STATIC_LIB}
)

add_library(beekeeper_rust STATIC IMPORTED GLOBAL)
add_dependencies(beekeeper_rust beekeeper_rust_build)
set_target_properties(beekeeper_rust PROPERTIES
    IMPORTED_LOCATION             ${BEEKEEPER_RUST_STATIC_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
    INTERFACE_LINK_LIBRARIES      "beekeeper_core_minimal;fc_crypto_bridge"
)

INSTALL( FILES ${BEEKEEPER_RUST_STATIC_LIB} DESTINATION lib )
INSTALL( FILES ${BEEKEEPER_RUST_HEADERS}    DESTINATION "include/beekeeper_rs" )
