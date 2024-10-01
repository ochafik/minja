/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include <fuzztest/fuzztest.h>
#include <fuzztest/grammars/json_grammar.h>
#include <gtest/gtest.h>
#include <minja.hpp>
#include <chat-template.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

using json = nlohmann::ordered_json;

static std::string render(const std::string & template_str, const json & bindings, const minja::Options & options) {
    auto root = minja::Parser::parse(template_str, options);
    auto context = minja::Context::make(bindings);
    std::string actual;
    try {
        actual = root->render(context);
    } catch (const std::runtime_error & e) {
        actual = "ERROR: " + std::string(e.what());
    }
    return actual;
}

// Dumps `{"a": 1}` as `"{\"a\": 1}"`, unlike nlohmann::json::dump which would dump it as `"{\"a\":1}"`.
static std::string dump(const json & j) {
  return minja::Value(j).dump(-1, /* to_json= */ true);
}

void TestRenderDoesNotCrash(const std::string& template_str, const std::string& json_str) {
    try {
        auto unused = render(template_str, json::parse(json_str), {});
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

void TestRenderJsonDoesNotCrash(const std::string & x)   {
    EXPECT_EQ(dump(json::parse(x)), render("{{ x | tojson }}", {{"x", json::parse(x)}}, {}));
}

void TestChatTemplate(const std::string& template_str, const std::string& messages_json, const std::string& tools_json) {
    try {
        minja::chat_template tmpl(template_str, "<|start|>", "<|end|>");
        auto messages = json::parse(messages_json);
        auto tools = json::parse(tools_json);
        auto unused = tmpl.apply(messages, tools, true, {});
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

FUZZ_TEST(JinjaFuzzTest, TestRenderDoesNotCrash)
    // .WithSeeds({
    //     {"{% for x in range(10) | odd %}{% if x % 3 == 0 %}{{ x * 100 }}{% endif %}{% endfor %}", {"x", nullptr}},
    //     {"{{ x.y[z]() - 1 }}", {}},
    //     {"{% if 1 %}{# booh #}{% endif %}", {}},
    //     {"{{ }}", {}},
    //     {"{% %}", {}},
    // })
    .WithDomains(
        fuzztest::Arbitrary<std::string>().WithMaxSize(1000),
        fuzztest::InJsonGrammar()
    );
FUZZ_TEST(JinjaFuzzTest, TestRenderJsonDoesNotCrash)
    // .WithSeeds({
    //     {"null"},
    //     {"[]"},
    //     {"[null]"},
    //     {"[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]"},
    //     {"{\"a\": [null]}"},
    // })
    .WithDomains(fuzztest::InJsonGrammar());

FUZZ_TEST(JinjaFuzzTest, TestChatTemplate)
    .WithDomains(
        fuzztest::Arbitrary<std::string>().WithMaxSize(1000),
        fuzztest::InJsonGrammar(),
        fuzztest::InJsonGrammar()
    );
