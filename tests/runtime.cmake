if(NOT COMMAND catch_discover_tests)
  find_package(grevir-test-support CONFIG REQUIRED)
endif()

add_executable(grevir_core_runtime
  runtime/lifecycle_test.cpp runtime/state_test.cpp runtime/other_translation_unit.cpp)
target_link_libraries(grevir_core_runtime PRIVATE grevir::core Catch2::Catch2WithMain)
set_target_properties(grevir_core_runtime PROPERTIES CXX_EXTENSIONS OFF)
catch_discover_tests(grevir_core_runtime
  TEST_PREFIX "core.mock."
  PROPERTIES LABELS "core-mock" TIMEOUT 10)
