# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT
foreach(test
    test-syntax
    test-chat-template
)
    add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE nlohmann_json::nlohmann_json)
    add_test(
        NAME ${test}
        COMMAND $<TARGET_FILE:${test}>
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    
endforeach()