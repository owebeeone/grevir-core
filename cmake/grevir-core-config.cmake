include(CMakeFindDependencyMacro)
find_dependency(grevir-base CONFIG)
include("${CMAKE_CURRENT_LIST_DIR}/GrevirCoreTargets.cmake")
get_filename_component(_grevir_core_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(GREVIR_IRQGEN_TOOL_DIR
  "${_grevir_core_prefix}/share/grevir-core/tools/grevir_irqgen")
include("${CMAKE_CURRENT_LIST_DIR}/GrevirInterruptBindings.cmake")
