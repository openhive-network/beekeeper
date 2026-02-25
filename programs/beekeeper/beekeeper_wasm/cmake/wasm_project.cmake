# Minimal WASM build configuration — no FC, no Boost, no OpenSSL.
# All crypto primitives are provided by JS callbacks.

set(WASM_RUNTIME_COMPONENT_NAME "wasm_runtime_components")

function( DEFINE_WASM_TARGET_FOR wasm_target_basename )
  set(options WASM_USE_FS)
  set(oneValueArgs TARGET_ENVIRONMENT)
  set(multiValueArgs LINK_LIBRARIES LINK_OPTIONS)
  cmake_parse_arguments(PARSE_ARGV 0 arg
    "${options}" "${oneValueArgs}" "${multiValueArgs}"
  )

  set( exec_wasm_name "${wasm_target_basename}.${arg_TARGET_ENVIRONMENT}" )
  set( exec_common_name "${wasm_target_basename}.common" )

  message(NOTICE "Configuring '${exec_wasm_name}'")

  IF ("${arg_TARGET_ENVIRONMENT}" STREQUAL "web")
    MESSAGE( STATUS "Chosen web target environment")
    SET ( NODE_ENV 0 )
    SET ( WASM_ENV "web" )
  ELSE()
    MESSAGE( STATUS "Chosen node target environment")
    SET ( NODE_ENV 1 )
    SET ( WASM_ENV "node" )
  ENDIF()

  ADD_EXECUTABLE( ${exec_wasm_name} ${SOURCES} )

  target_include_directories( ${exec_wasm_name} PUBLIC ${INCLUDES} )

  target_compile_options( ${exec_wasm_name} PUBLIC
    -Oz
  )

  target_link_libraries( ${exec_wasm_name} PUBLIC embind )

  target_link_options( ${exec_wasm_name} PUBLIC
    -Oz
    -sDISABLE_EXCEPTION_CATCHING=0
    -sEXPORT_EXCEPTION_HANDLING_HELPERS=1
    -sEXCEPTION_STACK_TRACES=1
    -sELIMINATE_DUPLICATE_FUNCTIONS=1
    -sMODULARIZE=1 -sSINGLE_FILE=0 -sUSE_ES6_IMPORT_META=1
    -sEXPORT_ES6=1 -sINITIAL_MEMORY=67108864 -sWASM_ASYNC_COMPILATION=1
    -sNO_FILESYSTEM=1
    -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=65536
    --minify=0 --emit-symbol-map -sENVIRONMENT=${WASM_ENV}
    --emit-tsd "${CMAKE_CURRENT_BINARY_DIR}/${exec_common_name}.d.ts"
  )

  if (arg_LINK_OPTIONS)
    target_link_options( ${exec_wasm_name} PUBLIC ${arg_LINK_OPTIONS})
  endif()

  if (arg_LINK_LIBRARIES)
    target_link_libraries( ${exec_wasm_name} PUBLIC ${arg_LINK_LIBRARIES})
  endif()

  set_target_properties( ${exec_wasm_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${arg_TARGET_ENVIRONMENT}"
    OUTPUT_NAME "${exec_common_name}.js"
  )

  INSTALL( FILES "${CMAKE_BINARY_DIR}/${arg_TARGET_ENVIRONMENT}/${exec_common_name}.js"
    RENAME "${exec_wasm_name}.js"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )

  INSTALL( FILES "${CMAKE_BINARY_DIR}/${arg_TARGET_ENVIRONMENT}/${exec_common_name}.wasm"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )

  INSTALL( FILES  "${CMAKE_CURRENT_BINARY_DIR}/${exec_common_name}.d.ts"
    COMPONENT "${WASM_RUNTIME_COMPONENT_NAME}"
    DESTINATION .
  )
endfunction()
