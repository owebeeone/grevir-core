file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS RELATIVE
  "${PROJECT_SOURCE_DIR}/src" "${PROJECT_SOURCE_DIR}/src/*.hpp"
  "${PROJECT_SOURCE_DIR}/src/*.h")
set(header_sources)
foreach(header IN LISTS public_headers)
  string(MAKE_C_IDENTIFIER "${header}" identifier)
  set(source "${CMAKE_CURRENT_BINARY_DIR}/${identifier}.cpp")
  file(WRITE "${source}" "#include <${header}>\n")
  list(APPEND header_sources "${source}")
endforeach()

add_library(grevir_core_compile OBJECT native_compile.cpp
  dependent_module_test.cpp resource_claims_test.cpp singleton_test.cpp parameter_index_test.cpp
  ${header_sources})
target_link_libraries(grevir_core_compile PRIVATE grevir::core)
set_target_properties(grevir_core_compile PROPERTIES CXX_EXTENSIONS OFF)

# These are compiler-only checks, not a runtime test harness. Restrict the
# driver flags to the compiler families that understand them.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(AppleClang|Clang|GNU)$")
  message(FATAL_ERROR "Core conflict probes currently require a Clang/GNU driver")
endif()
add_custom_target(grevir_core_claim_checks ALL
  COMMAND "${CMAKE_COMMAND}"
    "-DCXX=${CMAKE_CXX_COMPILER}"
    "-DCORE_INCLUDE=${PROJECT_SOURCE_DIR}/src"
    "-DBASE_INCLUDES=$<TARGET_PROPERTY:grevir::base,INTERFACE_INCLUDE_DIRECTORIES>"
    "-DCASE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/claim_cases.cpp"
    "-DLOG_DIR=${CMAKE_CURRENT_BINARY_DIR}/claim-results"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_claims.cmake"
  COMMENT "Checking application diagnostics and parameter index bounds"
  VERBATIM)
