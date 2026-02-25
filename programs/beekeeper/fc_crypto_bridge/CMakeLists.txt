file(GLOB HEADERS "include/fc_crypto_bridge/*.hpp")

add_library( fc_crypto_bridge
             fc_crypto_provider.cpp
             ${HEADERS}
           )

target_link_libraries( fc_crypto_bridge beekeeper_core_minimal fc )
target_include_directories( fc_crypto_bridge PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

INSTALL( TARGETS
   fc_crypto_bridge

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/fc_crypto_bridge" )
