/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
#include <fuzztest/fuzztest.h>
#include <fuzztest/grammars/json_grammar.h>
#include <gtest/gtest.h>
#include <minja.hpp>
#include <chat-template.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

// Helper function to check if a string is valid JSON
bool isValidJson(const std::string& str) {
    try {
        nlohmann::json::parse(str);
        return true;
    } catch (nlohmann::json::parse_error&) {
        return false;
    }
}

// Test raw Jinja templating
void TestRawJinjaTemplating(const std::string& template_str, const std::string& json_str) {
    if (!isValidJson(json_str)) return; // Skip invalid JSON inputs

    try {
        auto tmpl = minja::Parser::parse(template_str, {});
        auto json_data = nlohmann::json::parse(json_str);
        auto context = minja::Context::make(minja::Value(json_data));
        auto result = tmpl->render(context);

        // Basic sanity checks
        ASSERT_FALSE(result.empty());
        ASSERT_LE(result.length(), template_str.length() + json_str.length());
    } catch (const std::exception& e) {
        // It's okay if parsing fails for some inputs, but we don't want crashes
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

// Test chat template functionality
void TestChatTemplate(const std::string& template_str, const std::string& messages_json, const std::string& tools_json) {
    if (!isValidJson(messages_json) || !isValidJson(tools_json)) return; // Skip invalid JSON inputs

    try {
        minja::chat_template tmpl(template_str, "<|start|>", "<|end|>");
        auto messages = nlohmann::json::parse(messages_json);
        auto tools = nlohmann::json::parse(tools_json);
        auto result = tmpl.apply(messages, tools, true, {});

        // Basic sanity checks
        ASSERT_FALSE(result.empty());
        ASSERT_TRUE(result.find("<|start|>") != std::string::npos);
        ASSERT_TRUE(result.find("<|end|>") != std::string::npos);
    } catch (const std::exception& e) {
        // It's okay if parsing fails for some inputs, but we don't want crashes
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

// Define the fuzz tests
FUZZ_TEST(TestRawJinjaTemplating);
FUZZ_TEST(TestChatTemplate);

// Optional: Add domain constraints to generate more meaningful inputs
FUZZ_TEST(TestRawJinjaTemplating)
    .WithDomains(
        fuzztest::Arbitrary<std::string>().WithMaxSize(1000),
        fuzztest::Arbitrary<std::string>().WithMaxSize(1000)
    );

FUZZ_TEST(TestChatTemplate)
    .WithDomains(
        fuzztest::Arbitrary<std::string>().WithMaxSize(1000),
        fuzztest::Arbitrary<std::string>().WithMaxSize(1000),
        fuzztest::Arbitrary<std::string>().WithMaxSize(1000)
    );