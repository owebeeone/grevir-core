if(CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR "GREVIR_BUILD_HOST_TESTS requires a native compiler")
endif()

option(GREVIR_FETCH_TEST_DEPENDENCIES "Allow setup to download pinned host test dependencies" OFF)
set(GREVIR_CATCH2_SOURCE_DIR "" CACHE PATH "Existing Catch2 3.8.1 source for offline setup")
if(GREVIR_CATCH2_SOURCE_DIR)
  add_subdirectory("${GREVIR_CATCH2_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/catch2" EXCLUDE_FROM_ALL)
  list(APPEND CMAKE_MODULE_PATH "${GREVIR_CATCH2_SOURCE_DIR}/extras")
else()
  find_package(Catch2 3.8.1 EXACT CONFIG QUIET)
  if(NOT Catch2_FOUND)
    if(NOT GREVIR_FETCH_TEST_DEPENDENCIES)
      message(FATAL_ERROR
        "Host tests need Catch2 3.8.1. Supply an installed package, set GREVIR_CATCH2_SOURCE_DIR, or explicitly enable GREVIR_FETCH_TEST_DEPENDENCIES for setup.")
    endif()
    include(FetchContent)
    if(POLICY CMP0135)
      cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(Catch2
      URL https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.8.1
      URL_HASH SHA256=18b3f70ac80fccc340d8c6ff0f339b2ae64944782f8d2fca2bd705cf47cadb79
      TLS_VERIFY TRUE)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  endif()
endif()

add_executable(grevir_core_runtime
  runtime/lifecycle_test.cpp runtime/state_test.cpp runtime/other_translation_unit.cpp)
target_link_libraries(grevir_core_runtime PRIVATE grevir::core Catch2::Catch2WithMain)
set_target_properties(grevir_core_runtime PROPERTIES CXX_EXTENSIONS OFF)
include(Catch)
catch_discover_tests(grevir_core_runtime
  TEST_PREFIX "core.mock."
  PROPERTIES LABELS "core-mock" TIMEOUT 10)
