file(GLOB HEADERS "include/core_minimal/*.hpp")

add_library( beekeeper_core_minimal
             wallet.cpp
             session.cpp
             beekeeper.cpp
             ${HEADERS}
           )

target_link_libraries( beekeeper_core_minimal fc )
target_include_directories( beekeeper_core_minimal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )

INSTALL( TARGETS
   beekeeper_core_minimal

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
INSTALL( FILES ${HEADERS} DESTINATION "include/core_minimal" )
