file(GLOB HEADERS "include/beekeeper/*.hpp")

# Static library with beekeeper implementation (used by both executable and tests)
add_library(beekeeper_lib STATIC
    beekeeper_app.cpp
    beekeeper_instance.cpp
    beekeeper_wallet_api.cpp
    time_manager.cpp
    extended_api.cpp
    session_manager.cpp
    ${HEADERS}
)

target_link_libraries(beekeeper_lib PUBLIC
    appbase
    webserver_plugin
    app_status_api_plugin
    hive_utilities
    beekeeper_core
    beekeeper_protocol_config
)

target_include_directories(beekeeper_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Executable
add_executable(beekeeper main.cpp)

target_link_libraries(beekeeper beekeeper_lib)

target_include_directories(beekeeper PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Enable static linking for portability
if(NOT APPLE)
    target_link_options(beekeeper PRIVATE "-static-libstdc++" "-static-libgcc")
endif()

# For proper crash stacktrace
set_target_properties(beekeeper PROPERTIES ENABLE_EXPORTS TRUE)

if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

# https://stackoverflow.com/a/40922025/11738218
OPTION(ENABLE_PROFILING "If set additional flags are passed to compiler" OFF)
if(ENABLE_PROFILING)
    SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg" )
    SET( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg" )
    SET( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg" )
    message( INFO "Profiling enabled" )
endif()

