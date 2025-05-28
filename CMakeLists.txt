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
)

foreach(TEST_GROUP IN LISTS TEST_GROUPS)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/${TEST_GROUP})
    file(GLOB TEST_FILES ${CMAKE_SOURCE_DIR}/tests/${TEST_GROUP}/*.ika)
    foreach(TEST_FILE IN LISTS TEST_FILES)
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
        set(OUTPUT_FILE ${CMAKE_BINARY_DIR}/tests/${TEST_GROUP}/${TEST_NAME}_${CMAKE_BUILD_TYPE}.txt)
        add_test(
            NAME ${TEST_GROUP}_${TEST_NAME}_${CMAKE_BUILD_TYPE}
            COMMAND ikac -o ${OUTPUT_FILE} ${TEST_FILE}
        )
        set_tests_properties(${TEST_GROUP}_${TEST_NAME}_${CMAKE_BUILD_TYPE}
            PROPERTIES LABELS ${TEST_GROUP}
        )
    endforeach()
endforeach()
