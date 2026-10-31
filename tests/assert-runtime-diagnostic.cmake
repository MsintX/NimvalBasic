if(NOT DEFINED PROGRAM OR NOT DEFINED SOURCE OR NOT DEFINED EXPECTED)
    message(FATAL_ERROR "PROGRAM, SOURCE and EXPECTED are required")
endif()
execute_process(
    COMMAND "${PROGRAM}" "${SOURCE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
set(output "${stdout}${stderr}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Expected runtime success with a warning, exit=${result}:
${output}")
endif()
string(FIND "${output}" "${EXPECTED}" found)
if(found EQUAL -1)
    message(FATAL_ERROR "Expected runtime diagnostic '${EXPECTED}', got:
${output}")
endif()
