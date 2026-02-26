file(GLOB HEADERS "include/core_minimal/*.hpp")

add_library( beekeeper_core_minimal
             wallet.cpp
             session.cpp
             beekeeper.cpp
             crypto_provider_impl.cpp
             memory_storage.cpp
             ${HEADERS}
           )

# No external dependencies — core_minimal is pure C++17
target_include_directories( beekeeper_core_minimal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

INSTALL( TARGETS
   beekeeper_core_minimal

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/core_minimal" )
