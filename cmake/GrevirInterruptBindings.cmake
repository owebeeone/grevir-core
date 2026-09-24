include_guard(GLOBAL)
include(CMakeParseArguments)
if(NOT DEFINED GREVIR_IRQGEN_TOOL_DIR)
  get_filename_component(GREVIR_IRQGEN_TOOL_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../tools/grevir_irqgen" ABSOLUTE)
endif()

# A target supplies its normal include directories, definitions and linked
# interface libraries before calling this function. One probe object is built
# with those same settings, then a successful transaction publishes the source.
function(grevir_add_interrupt_bindings)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  cmake_parse_arguments(IRQ "" "TARGET;APPLICATION_HEADER;BACKEND;TARGET_ID;BOARD;COMPILER_ID" "" ${ARGN})
  foreach(required IN ITEMS TARGET APPLICATION_HEADER BACKEND TARGET_ID BOARD COMPILER_ID)
    if(NOT IRQ_${required})
      message(FATAL_ERROR "grevir_add_interrupt_bindings requires ${required}")
    endif()
  endforeach()
  if(NOT TARGET ${IRQ_TARGET})
    message(FATAL_ERROR "interrupt firmware target does not exist: ${IRQ_TARGET}")
  endif()
  get_filename_component(_app_abs "${IRQ_APPLICATION_HEADER}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  if(NOT EXISTS "${_app_abs}")
    message(FATAL_ERROR "interrupt application header does not exist: ${_app_abs}")
  endif()
  set(_base "${CMAKE_CURRENT_BINARY_DIR}/grevir-irq/${IRQ_TARGET}")
  set(_out "${_base}/$<CONFIG>")
  file(MAKE_DIRECTORY "${_base}")
  set(_probe "${_base}/probe.cpp")
  file(WRITE "${_probe}" "#include \"${_app_abs}\"\n#include <grevir/interrupt/probe_section.hpp>\nGREVIR_EMIT_IRQ_PROBE(GrevirApplication);\n")
  set(_probe_target "${IRQ_TARGET}_grevir_irq_probe")
  add_library(${_probe_target} OBJECT "${_probe}")
  target_compile_features(${_probe_target} PRIVATE cxx_std_23)
  target_compile_definitions(${_probe_target} PRIVATE GREVIR_IRQ_PROBE=1)
  foreach(property IN ITEMS LINK_LIBRARIES INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS)
    get_target_property(_value ${IRQ_TARGET} ${property})
    if(_value)
      if(property STREQUAL "LINK_LIBRARIES")
        target_link_libraries(${_probe_target} PRIVATE ${_value})
      elseif(property STREQUAL "INCLUDE_DIRECTORIES")
        target_include_directories(${_probe_target} PRIVATE ${_value})
      elseif(property STREQUAL "COMPILE_DEFINITIONS")
        target_compile_definitions(${_probe_target} PRIVATE ${_value})
      else()
        target_compile_options(${_probe_target} PRIVATE ${_value})
      endif()
    endif()
  endforeach()
  set(_ready "${_out}/grevir_irq_ready.json")
  set(_json "${_out}/grevir_generated_irq_plan_${IRQ_BACKEND}.json")
  set(_header "${_out}/grevir_generated_irq_bindings_${IRQ_BACKEND}.hpp")
  set(_source "${_out}/grevir_generated_irq_bindings_${IRQ_BACKEND}.cpp")
  set(_tool_dependencies
    "${GREVIR_IRQGEN_TOOL_DIR}/transaction.py"
    "${GREVIR_IRQGEN_TOOL_DIR}/__main__.py"
    "${GREVIR_IRQGEN_TOOL_DIR}/object_section.py"
    "${GREVIR_IRQGEN_TOOL_DIR}/protocol.py"
    "${GREVIR_IRQGEN_TOOL_DIR}/emit.py"
    "${GREVIR_IRQGEN_TOOL_DIR}/verify.py")
  add_custom_command(OUTPUT "${_ready}"
    BYPRODUCTS "${_json}" "${_header}" "${_source}"
    COMMAND "${Python3_EXECUTABLE}" -B
      "${GREVIR_IRQGEN_TOOL_DIR}/transaction.py"
      --object "$<TARGET_OBJECTS:${_probe_target}>"
      --out-dir "${_out}" --backend "${IRQ_BACKEND}"
      --target "${IRQ_TARGET_ID}" --board "${IRQ_BOARD}"
      --compiler "${IRQ_COMPILER_ID}"
      --application-header "${_app_abs}"
    DEPENDS ${_probe_target} "${_app_abs}" ${_tool_dependencies}
    VERBATIM)
  add_custom_target("${IRQ_TARGET}_grevir_irq_ready" DEPENDS "${_ready}")
  add_custom_target("${IRQ_TARGET}_grevir_irq_verified"
    COMMAND "${Python3_EXECUTABLE}" -B
      "${GREVIR_IRQGEN_TOOL_DIR}/verify.py"
      --out-dir "${_out}" --backend "${IRQ_BACKEND}"
      --target "${IRQ_TARGET_ID}" --board "${IRQ_BOARD}"
      --compiler "${IRQ_COMPILER_ID}"
    DEPENDS "${IRQ_TARGET}_grevir_irq_ready"
    VERBATIM)
  add_dependencies(${IRQ_TARGET} "${IRQ_TARGET}_grevir_irq_verified")
  target_sources(${IRQ_TARGET} PRIVATE "${_source}")
  target_include_directories(${IRQ_TARGET} PRIVATE "${_out}")
  target_compile_definitions(${IRQ_TARGET} PRIVATE
    "GREVIR_GENERATED_IRQ_HEADER=\"grevir_generated_irq_bindings_${IRQ_BACKEND}.hpp\"")
endfunction()
