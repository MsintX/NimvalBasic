if(NOT DEFINED PROGRAM OR NOT DEFINED SOURCE OR NOT DEFINED EXPECTED)
    message(FATAL_ERROR "PROGRAM, SOURCE and EXPECTED are required")
endif()
execute_process(
    COMMAND "${PROGRAM}" --check "${SOURCE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
set(output "${stdout}${stderr}")
if(result EQUAL 0)
    message(FATAL_ERROR "Expected static check to fail, but it succeeded")
endif()
string(FIND "${output}" "${EXPECTED}" found)
if(found EQUAL -1)
    message(FATAL_ERROR "Expected diagnostic '${EXPECTED}', got:\n${output}")
endif()
