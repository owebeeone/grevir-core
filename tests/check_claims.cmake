file(MAKE_DIRECTORY "${LOG_DIR}")
set(flags -std=c++23 -DHAS_STD_LIB=1 -fsyntax-only "-I${CORE_INCLUDE}")
foreach(directory IN LISTS BASE_INCLUDES)
  list(APPEND flags "-I${directory}")
endforeach()

# Prove that the same compiler, includes and source work before accepting any
# expected failure. Full output remains available for diagnosing each probe.
foreach(case RANGE 0 11)
  execute_process(COMMAND "${CXX}" ${flags} "-DCASE_ID=${case}" "${CASE_SOURCE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  file(WRITE "${LOG_DIR}/case-${case}.log" "${output}${errors}")
  if(case LESS 2)
    if(NOT result STREQUAL "0")
      message(FATAL_ERROR "Positive claim case ${case} failed:\n${output}${errors}")
    endif()
  else()
    if(NOT result MATCHES "^[1-9][0-9]*$")
      message(FATAL_ERROR "Claim case ${case}: expected compiler rejection, got ${result}")
    endif()
    if(case GREATER 9)
      set(diagnostic "GREVIR_CORE_DEPENDENCY_CYCLE")
    elseif(case EQUAL 8)
      set(diagnostic "Found resource conflict in same claim[.]")
    elseif(case EQUAL 9)
      set(diagnostic "Incorrect range, End must be strictly after Begin[.]")
    else()
      set(diagnostic "Application has resource conflict")
    endif()
    if(NOT errors MATCHES "static assertion failed[^\n]*${diagnostic}")
      message(FATAL_ERROR "Claim case ${case} failed without its expected diagnostic:\n${errors}")
    endif()
  endif()
endforeach()
message(STATUS "Core compile probes: 2 valid applications and 10 expected failures passed")

function(check_parameter_index index empty expect_success)
  execute_process(COMMAND "${CXX}" ${flags}
    "-DPARAM_INDEX=${index}" "-DEMPTY_PARAMS=${empty}"
    "${CMAKE_CURRENT_LIST_DIR}/parameter_index_probe.cpp"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  file(WRITE "${LOG_DIR}/parameter-${index}-empty-${empty}.log" "${output}${errors}")
  if(expect_success)
    if(NOT result STREQUAL "0")
      message(FATAL_ERROR "Valid parameter index ${index} failed:\n${output}${errors}")
    endif()
  elseif(NOT result MATCHES "^[1-9][0-9]*$" OR
      NOT errors MATCHES "static assertion failed[^\n]*GREVIR_CORE_PARAMETER_INDEX_OUT_OF_RANGE")
    message(FATAL_ERROR "Parameter index ${index}, empty=${empty}: expected bounds diagnostic; got ${result}:\n${output}${errors}")
  endif()
endfunction()

# Positive controls use the same source, compiler and flags as the invalid cases.
check_parameter_index(0 0 TRUE)
check_parameter_index(1 0 TRUE)
check_parameter_index(2 0 FALSE)
check_parameter_index(99 0 FALSE)
check_parameter_index(0 1 FALSE)
message(STATUS "Parameter index probes: 2 valid selections and 3 expected bounds failures passed")
