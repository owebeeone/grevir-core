cmake_minimum_required(VERSION 3.20)
project(grevir_core VERSION 0.1.0 LANGUAGES CXX)

if(NOT TARGET grevir::base)
  find_package(grevir-base CONFIG REQUIRED)
endif()

add_library(grevir_core INTERFACE)
add_library(grevir::core ALIAS grevir_core)
set_target_properties(grevir_core PROPERTIES EXPORT_NAME core)
target_compile_features(grevir_core INTERFACE cxx_std_23)
target_link_libraries(grevir_core INTERFACE grevir::base)
target_include_directories(grevir_core INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>)

include(GNUInstallDirs)
install(TARGETS grevir_core EXPORT GrevirCoreTargets)
install(DIRECTORY src/ DESTINATION include FILES_MATCHING
  PATTERN "*.hpp" PATTERN "*.h")
install(EXPORT GrevirCoreTargets NAMESPACE grevir::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grevir-core)
install(FILES cmake/grevir-core-config.cmake cmake/GrevirInterruptBindings.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/grevir-core)
install(DIRECTORY tools/grevir_irqgen/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/grevir-core/tools/grevir_irqgen
  FILES_MATCHING PATTERN "*.py" PATTERN "__pycache__" EXCLUDE)
install(FILES LICENSE.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/grevir-core)

option(GREVIR_BUILD_COMPILE_CHECKS "Compile native headers and template users" OFF)
option(GREVIR_BUILD_HOST_TESTS "Run deterministic native behavior tests" OFF)
if(GREVIR_BUILD_HOST_TESTS)
  enable_testing()
endif()
if(GREVIR_BUILD_COMPILE_CHECKS OR GREVIR_BUILD_HOST_TESTS)
  add_subdirectory(tests)
endif()
