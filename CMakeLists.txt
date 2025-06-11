cmake_minimum_required(VERSION 3.15)
project(ika-compiler LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type specified, defaulting to Release.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB_RECURSE IKA_SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.c
)

add_executable(ikac ${IKA_SOURCES})

target_compile_options(ikac PRIVATE
    -include ${CMAKE_SOURCE_DIR}/src/common.h
    -Wall
    -Wextra
    -Wpedantic
)

# Testing
enable_testing()

set(TEST_GROUPS
    basic
    control-flow
    function
    pointer
    array
    struct
    enum
    preprocessor
)

set(IKAC $<TARGET_FILE:ikac>)

function(add_ika_test TEST_GROUP TEST_FILE)
    get_filename_component(TEST_NAME  ${TEST_FILE} NAME_WE)

    # Paths that differ per‐test
    set(BIN_OUT  ${CMAKE_BINARY_DIR}/tests/${TEST_GROUP}/${TEST_NAME}${CMAKE_EXECUTABLE_SUFFIX})
    set(RUN_OUT  ${CMAKE_BINARY_DIR}/tests/${TEST_GROUP}/${TEST_NAME}.out)
    set(EXPECTED ${CMAKE_SOURCE_DIR}/tests/${TEST_GROUP}/${TEST_NAME}.txt)

    if(TEST_GROUP STREQUAL "preprocessor")
        add_test(
            NAME     ${TEST_GROUP}_${TEST_NAME}_${CMAKE_BUILD_TYPE}
            COMMAND  ${CMAKE_COMMAND}
                     -DIKAC=${IKAC}
                     -DSRC=${TEST_FILE}
                     -DOUTPUT=${RUN_OUT}
                     -DEXPECTED=${EXPECTED}
                     -P  ${CMAKE_SOURCE_DIR}/cmake/RunPreprocessorTest.cmake
        )
    else()
        add_test(
            NAME     ${TEST_GROUP}_${TEST_NAME}_${CMAKE_BUILD_TYPE}
            COMMAND  ${CMAKE_COMMAND}
                     -DIKAC=${IKAC}
                     -DSRC=${TEST_FILE}
                     -DBIN=${BIN_OUT}
                     -DOUTPUT=${RUN_OUT}
                     -DEXPECTED=${EXPECTED}
                     -P  ${CMAKE_SOURCE_DIR}/cmake/RunTest.cmake
        )
    endif()

    # Nice filtering in ctest -L <label>
    set_tests_properties(${TEST_GROUP}_${TEST_NAME}_${CMAKE_BUILD_TYPE}
        PROPERTIES LABELS "${TEST_GROUP}"
    )
endfunction()

foreach(TEST_GROUP IN LISTS TEST_GROUPS)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/${TEST_GROUP})
    file(GLOB TEST_FILES "${CMAKE_SOURCE_DIR}/tests/${TEST_GROUP}/*.ika")
    foreach(TEST_FILE IN LISTS TEST_FILES)
        add_ika_test("${TEST_GROUP}" "${TEST_FILE}")
    endforeach()
endforeach()
