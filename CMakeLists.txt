# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.14)

project(minja VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Note: tests require C++14 because google/fuzztest depends on a version of gtest that requires it
# (and we don't want to use an older version of fuzztest)
# Examples are built w/ C++11 to check the compatibility of the library.
set(CMAKE_CXX_STANDARD 11)

include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(json URL https://github.com/nlohmann/json/archive/refs/heads/develop.zip)
FetchContent_MakeAvailable(json)

# Fetch google/fuzztest (and indirectly, gtest)
FetchContent_Declare(fuzztest URL https://github.com/google/fuzztest/archive/refs/heads/main.zip)
FetchContent_MakeAvailable(fuzztest)

# Use ccache if installed
find_program(CCACHE_PATH ccache)
if (CCACHE_PATH)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PATH})
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PATH})
endif()

# Release build by default
if (NOT XCODE AND NOT MSVC AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Create a python venv w/ the required dependencies
find_package(Python COMPONENTS Interpreter REQUIRED)
function(setup_python_environment)
    set(VENV_DIR "${CMAKE_BINARY_DIR}/venv")

    # Create a virtual environment if it doesnt exist yet
    if(EXISTS ${VENV_DIR})
        set(Python_EXECUTABLE ${VENV_DIR}/bin/python PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND ${Python_EXECUTABLE} -m venv ${VENV_DIR}
        RESULT_VARIABLE VENV_RESULT
    )
    if(NOT VENV_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to create virtual environment")
    endif()

    if(WIN32)
        set(VENV_PYTHON "${VENV_DIR}/Scripts/python.exe")
    else()
        set(VENV_PYTHON "${VENV_DIR}/bin/python")
    endif()

    execute_process(
        COMMAND ${VENV_PYTHON} -m pip install -r "${CMAKE_SOURCE_DIR}/requirements.txt"
        RESULT_VARIABLE PIP_RESULT
    )
    if(NOT PIP_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to install Python dependencies")
    endif()

    set(Python_EXECUTABLE ${VENV_PYTHON} PARENT_SCOPE)
endfunction()
setup_python_environment()

find_program(CPPCHECK cppcheck)
if(CPPCHECK)
  set(CMAKE_CXX_CPPCHECK "${CPPCHECK}" -i ${json_SOURCE_DIR}/include/nlohmann/json.hpp)
  message(STATUS "cppcheck found: ${CPPCHECK}")
endif()

message(STATUS "${fuzztest_BINARY_DIR}: ${${fuzztest_BINARY_DIR}}")
include_directories(
    include/minja
    ${json_SOURCE_DIR}/include
)

add_subdirectory(examples)

enable_testing()
add_subdirectory(tests)
