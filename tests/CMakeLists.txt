# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT

add_executable(test-syntax test-syntax.cpp)
target_compile_features(test-syntax PUBLIC cxx_std_17)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_definitions(test-syntax PUBLIC _CRT_SECURE_NO_WARNINGS)
    target_compile_options(gtest PRIVATE -Wno-language-extension-token)
endif()
target_link_libraries(test-syntax PRIVATE
    minja
    gtest_main
    gmock
)

if (WIN32)
    message(STATUS "Skipping test-chat-template on Win32")
else()
    add_executable(test-chat-template test-chat-template.cpp)
    target_compile_features(test-chat-template PUBLIC cxx_std_17)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        target_compile_definitions(test-chat-template PUBLIC _CRT_SECURE_NO_WARNINGS)
        target_compile_options(gtest PRIVATE -Wno-language-extension-token)
    endif()
    target_link_libraries(test-chat-template PRIVATE
        minja
        gtest_main
        gmock
    )
endif()

add_executable(test-polyfills test-polyfills.cpp)
target_compile_features(test-polyfills PUBLIC cxx_std_17)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_definitions(test-polyfills PUBLIC _CRT_SECURE_NO_WARNINGS)
    target_compile_options(gtest PRIVATE -Wno-language-extension-token)
endif()
# GCC/MinGW on Windows needs -Wa,-mbig-obj for large debug builds due to COFF section limits
if (MINGW AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(test-polyfills PRIVATE -Wa,-mbig-obj)
endif()
target_link_libraries(test-polyfills PRIVATE
    minja
    gtest_main
    gmock
)
if (NOT CMAKE_CROSSCOMPILING)
    gtest_discover_tests(test-syntax)
    if (NOT WIN32)
        gtest_discover_tests(test-chat-template)
    endif()
    add_test(NAME test-polyfills COMMAND test-polyfills)
    set_tests_properties(test-polyfills PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

add_executable(test-capabilities test-capabilities.cpp)
target_compile_features(test-capabilities PUBLIC cxx_std_17)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_definitions(test-capabilities PUBLIC _CRT_SECURE_NO_WARNINGS)
    target_compile_options(gtest PRIVATE -Wno-language-extension-token)
endif()
target_link_libraries(test-capabilities PRIVATE
    minja
    gtest_main
    gmock
)
add_test(NAME test-capabilities COMMAND test-capabilities)
set_tests_properties(test-capabilities PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_test(NAME test-syntax-jinja2 COMMAND test-syntax)
set_tests_properties(test-syntax-jinja2 PROPERTIES ENVIRONMENT "USE_JINJA2=1;PYTHON_EXECUTABLE=${Python_EXECUTABLE};PYTHONPATH=${CMAKE_SOURCE_DIR}")


add_executable(test-supported-template test-supported-template.cpp)
target_compile_features(test-supported-template PUBLIC cxx_std_17)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_definitions(test-supported-template PUBLIC _CRT_SECURE_NO_WARNINGS)
endif()
target_link_libraries(test-supported-template PRIVATE minja)

# https://huggingface.co/models?other=conversational
# https://huggingface.co/spaces/open-llm-leaderboard/open_llm_leaderboard#/?types=fine-tuned%2Cchat

set(MODEL_IDS
    # List of model IDs to test the chat template of.
    # For each of them, the tokenizer_config.json file will be fetched, and the template
    # will be used to render each of the (relevant) test contexts into a golden file with
    # the official Python jinja2 library. Then a test case will be created to run the C++
    # minja implementation on the same template and context, and compare the output with the golden.
    #
    # For Gated models, you'll need to run `huggingface-cli login` (and be granted access) to download their template.

    BEE-spoke-data/tFINE-900m-instruct-orpo
    CohereForAI/aya-expanse-8b
    CohereForAI/c4ai-command-r-plus
    CohereForAI/c4ai-command-r7b-12-2024
    Delta-Vector/Rei-12B
    HelpingAI/HAI-SER
    HuggingFaceTB/SmolLM2-1.7B-Instruct
    HuggingFaceTB/SmolLM3-3B
    Infinigence/Megrez-3B-Instruct
    LGAI-EXAONE/EXAONE-3.5-2.4B-Instruct
    MiniMaxAI/MiniMax-Text-01
    MiniMaxAI/MiniMax-VL-01
    NousResearch/Hermes-3-Llama-3.1-70B
    OnlyCheeini/greesychat-turbo
    OrionStarAI/Orion-14B-Chat
    PowerInfer/SmallThinker-3B-Preview
    PrimeIntellect/INTELLECT-1-Instruct
    Qwen/QVQ-72B-Preview
    Qwen/QwQ-32B
    Qwen/QwQ-32B-Preview
    Qwen/Qwen1.5-7B-Chat
    Qwen/Qwen2-VL-7B-Instruct
    Qwen/Qwen2.5-7B
    Qwen/Qwen2.5-7B-Instruct
    Qwen/Qwen2.5-Math-7B-Instruct
    Qwen/Qwen3-235B-A22B-Instruct-2507
    Qwen/Qwen3-235B-A22B-Thinking-2507
    Qwen/Qwen3-4B
    Qwen/Qwen3-Coder-30B-A3B-Instruct
    SakanaAI/TinySwallow-1.5B-Instruct
    THUDM/glm-4-9b-chat
    THUDM/glm-edge-1.5b-chat
    TheBloke/FusionNet_34Bx2_MoE-AWQ
    TinyLlama/TinyLlama-1.1B-Chat-v1.0
    UCLA-AGI/Mistral7B-PairRM-SPPO-Iter3
    abacusai/Fewshot-Metamath-OrcaVicuna-Mistral
    allenai/Llama-3.1-Tulu-3-8B
    arcee-ai/Virtuoso-Medium-v2
    avemio/GRAG-NEMO-12B-ORPO-HESSIAN-AI
    bfuzzy1/acheron-m1a-llama
    bofenghuang/vigogne-2-70b-chat
    bytedance-research/UI-TARS-72B-DPO
    carsenk/phi3.5_mini_exp_825_uncensored
    databricks/dbrx-instruct
    deepseek-ai/DeepSeek-R1-Distill-Llama-70B
    deepseek-ai/DeepSeek-V2-Lite
    deepseek-ai/DeepSeek-V2.5
    deepseek-ai/DeepSeek-V3
    deepseek-ai/deepseek-coder-7b-instruct-v1.5
    dicta-il/dictalm2.0-instruct
    ehristoforu/Falcon3-8B-Franken-Basestruct
    google/gemma-7b-it
    ibm-granite/granite-3.1-8b-instruct
    inclusionAI/Ling-Coder-lite
    indischepartij/MiniCPM-3B-OpenHermes-2.5-v2
    jinaai/ReaderLM-v2
    langgptai/qwen1.5-7b-chat-sa-v0.1
    llava-hf/llava-1.5-7b-hf
    meetkai/functionary-medium-v3.1
    meetkai/functionary-medium-v3.2
    meta-llama/Llama-2-7b-chat-hf
    meta-llama/Llama-3.1-8B-Instruct
    meta-llama/Llama-3.2-3B-Instruct
    meta-llama/Meta-Llama-3-8B-Instruct
    microsoft/Phi-3-medium-4k-instruct
    microsoft/Phi-3-mini-4k-instruct
    microsoft/Phi-3-small-8k-instruct
    microsoft/Phi-3.5-mini-instruct
    microsoft/Phi-3.5-vision-instruct
    microsoft/phi-4
    ministral/Ministral-3b-instruct
    mistralai/Codestral-22B-v0.1
    mistralai/Mistral-7B-Instruct-v0.1
    mistralai/Mistral-7B-Instruct-v0.3
    mistralai/Mistral-Large-Instruct-2411
    mistralai/Mistral-Nemo-Instruct-2407
    mistralai/Mistral-Small-24B-Instruct-2501
    mkurman/Qwen2.5-14B-DeepSeek-R1-1M
    mlabonne/AlphaMonarch-7B
    mlx-community/Josiefied-Qwen2.5-0.5B-Instruct-abliterated-v1-float32
    netcat420/MFANNv0.20
    nvidia/Eagle2-9B
    nvidia/Llama-3.1-Nemotron-70B-Instruct-HF
    onnx-community/DeepSeek-R1-Distill-Qwen-1.5B-ONNX
    openbmb/MiniCPM3-4B
    openchat/openchat-3.5-0106
    princeton-nlp/Mistral-7B-Instruct-DPO
    prithivMLmods/Bellatrix-Tiny-1.5B-R1
    prithivMLmods/Bellatrix-Tiny-1B-R1
    prithivMLmods/Bellatrix-Tiny-1B-v3
    rubenroy/Zurich-14B-GCv2-5m
    rubenroy/Zurich-7B-GCv2-5m
    sometimesanotion/Lamarck-14B-v0.7
    sthenno/tempesthenno-icy-0130
    teknium/OpenHermes-2.5-Mistral-7B
    tiiuae/Falcon3-10B-Instruct
    upstage/solar-pro-preview-instruct
    xwen-team/Xwen-7B-Chat
    zai-org/GLM-4.6

    # Broken, TODO:
    # ai21labs/AI21-Jamba-1.5-Large        # https://github.com/google/minja/issues/8
    # Almawave/Velvet-14B
    # deepseek-ai/DeepSeek-R1
    # deepseek-ai/DeepSeek-R1-Zero
    # fireworks-ai/llama-3-firefunction-v2 # https://github.com/google/minja/issues/7
    # HuggingFaceTB/SmolVLM-256M-Instruct
    # HuggingFaceTB/SmolVLM-500M-Instruct
    # HuggingFaceTB/SmolVLM-Instruct
    # unsloth/MiniMax-M2 # https://github.com/ochafik/minja/pull/7#issuecomment-3478459580
    # meta-llama/Llama-3.2-11B-Vision-Instruct
    # unsloth/DeepSeek-R1
)

if(WIN32)
    list(REMOVE_ITEM MODEL_IDS
        # Needs investigation (https://github.com/google/minja/issues/40)
        CohereForAI/c4ai-command-r7b-12-2024    
    )
endif()

# Create one test case for each {template, context} combination
file(GLOB CONTEXT_FILES "${CMAKE_SOURCE_DIR}/tests/contexts/*.json")
execute_process(
    COMMAND ${Python_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/fetch_templates_and_goldens.py
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CONTEXT_FILES}
        ${MODEL_IDS}
    OUTPUT_VARIABLE CHAT_TEMPLATE_TEST_CASES
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)
string(REPLACE "\n" ";" CHAT_TEMPLATE_TEST_CASES "${CHAT_TEMPLATE_TEST_CASES}")
list(LENGTH CHAT_TEMPLATE_TEST_CASES CHAT_TEMPLATE_TEST_CASES_COUNT)
message(STATUS "Found ${CHAT_TEMPLATE_TEST_CASES_COUNT} chat template test cases")
if (CHAT_TEMPLATE_TEST_CASES_COUNT LESS 10)
    message(ERROR "Not enough chat template test cases found")
endif()
foreach(test_case ${CHAT_TEMPLATE_TEST_CASES})
    separate_arguments(test_args UNIX_COMMAND "${test_case}")
    list(GET test_args -1 last_arg)
    string(REGEX REPLACE "^[^ ]+/([^ /\\]+)\\.[^.]+$" "\\1" test_name "${last_arg}")
    add_test(NAME test-supported-template-${test_name} COMMAND $<TARGET_FILE:test-supported-template> ${test_args})
    set_tests_properties(test-supported-template-${test_name} PROPERTIES SKIP_RETURN_CODE 127)
endforeach()

# Test to ensure no duplicate templates exist
add_test(
    NAME test-no-duplicate-templates
    COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_no_duplicate_templates.py ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

if (MINJA_FUZZTEST_ENABLED)
    if (MINJA_FUZZTEST_FUZZING_MODE)
        message(STATUS "Fuzzing mode enabled")
        fuzztest_setup_fuzzing_flags()
    endif()
    add_executable(test-fuzz test-fuzz.cpp)
    target_compile_features(test-fuzz PUBLIC cxx_std_17)
    target_include_directories(test-fuzz PRIVATE ${fuzztest_BINARY_DIR})
    target_link_libraries(test-fuzz PRIVATE nlohmann_json::nlohmann_json)
    link_fuzztest(test-fuzz)
    if (NOT CMAKE_CROSSCOMPILING)
        gtest_discover_tests(test-fuzz)
    endif()
endif()
