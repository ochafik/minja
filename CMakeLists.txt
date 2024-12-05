# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.14)

cmake_policy(SET CMP0135 NEW) # https://cmake.org/cmake/help/latest/policy/CMP0135.html

project(minja VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MINJA_TEST_GATED_MODELS    "minja: test gated models"    OFF)
if (MSVC)
    set(MINJA_FUZZTEST_ENABLED_DEFAULT OFF)
else()
    set(MINJA_FUZZTEST_ENABLED_DEFAULT ON)
endif()
option(MINJA_FUZZTEST_ENABLED       "minja: fuzztests enabled"  MINJA_FUZZTEST_ENABLED_DEFAULT)
option(MINJA_FUZZTEST_FUZZING_MODE  "minja: run fuzztests (if enabled) in fuzzing mode"     OFF)

# Note: tests require C++14 because google/fuzztest depends on a version of gtest that requires it
# (and we don't want to use an older version of fuzztest)
# Examples are built w/ C++11 to check the compatibility of the library.
set(CMAKE_CXX_STANDARD 11)

include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(json URL https://github.com/nlohmann/json/archive/refs/heads/develop.zip)
FetchContent_MakeAvailable(json)

if (MINJA_FUZZTEST_ENABLED)
    # Fetch google/fuzztest (and indirectly, gtest)
    FetchContent_Declare(fuzztest URL https://github.com/google/fuzztest/archive/refs/heads/main.zip)
    FetchContent_MakeAvailable(fuzztest)
else()
    # Fetch gtest
    FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/refs/heads/main.zip)
    FetchContent_MakeAvailable(googletest)
endif()

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
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(VENV_DIR "${CMAKE_BINARY_DIR}/venv")
if(WIN32)
    set(VENV_PYTHON "${VENV_DIR}/Scripts/python.exe")
else()
    set(VENV_PYTHON "${VENV_DIR}/bin/python")
endif()
execute_process(
    COMMAND ${Python3_EXECUTABLE} -m venv "${VENV_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(
    COMMAND ${VENV_PYTHON} -m pip install -r "${CMAKE_SOURCE_DIR}/requirements.txt"
    COMMAND_ERROR_IS_FATAL ANY)
set(Python_EXECUTABLE "${VENV_PYTHON}" CACHE FILEPATH "Path to Python executable in venv" FORCE)
message(STATUS "Python executable: ${Python_EXECUTABLE}")

find_program(CPPCHECK cppcheck)
if(CPPCHECK)
  set(CMAKE_CXX_CPPCHECK "${CPPCHECK}" -i ${json_SOURCE_DIR}/include/nlohmann/json.hpp)
  message(STATUS "cppcheck found: ${CPPCHECK}")
endif()

message(STATUS "${fuzztest_BINARY_DIR}: ${${fuzztest_BINARY_DIR}}")
include_directories(
    include/minja
    ${json_SOURCE_DIR}/include
    ${json_SOURCE_DIR}/include/nlohmann
)

add_subdirectory(examples)

enable_testing()
include(GoogleTest)
add_subdirectory(tests)
