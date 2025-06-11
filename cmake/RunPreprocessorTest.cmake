cmake_minimum_required(VERSION 3.15)

foreach(var IKAC SRC OUTPUT EXPECTED)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "RunTest.cmake: missing -D${var}=...")
    endif()
endforeach()

execute_process(
    COMMAND "${IKAC}" -E "${SRC}"
    OUTPUT_FILE "${OUTPUT}"
    RESULT_VARIABLE rc
)
if(rc)
    message(FATAL_ERROR "Pre-processing failed (${SRC})")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files --ignore-eol "${OUTPUT}" "${EXPECTED}"
    RESULT_VARIABLE diff_rc
)
if(diff_rc)
    message(FATAL_ERROR "Mismatch: ${SRC}")
endif()
