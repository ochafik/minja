
/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include "minja/chat-template.hpp"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <fstream>
#include <iostream>
#include <string>

using namespace minja;
using namespace testing;

static std::string render_python(const std::string & template_str, const chat_template_inputs & inputs) {
    json bindings = inputs.extra_context;
    bindings["messages"] = inputs.messages;
    bindings["tools"] = inputs.tools;
    bindings["add_generation_prompt"] = inputs.add_generation_prompt;
    json data {
        {"template", template_str},
        {"bindings", bindings},
        {"options", {
            {"trim_blocks", true},
            {"lstrip_blocks", true},
            {"keep_trailing_newline", false},
        }},
    };
    {
        std::ofstream of("data.json");
        of << data.dump(2);
        of.close();
    }

    auto pyExeEnv = getenv("PYTHON_EXECUTABLE");
    std::string pyExe = pyExeEnv ? pyExeEnv : "python3";

    std::remove("out.txt");
    auto res = std::system((pyExe + " -m scripts.render data.json out.txt").c_str());
    if (res != 0) {
        throw std::runtime_error("Failed to run python script with data: " + data.dump(2));
    }

    std::ifstream f("out.txt");
    std::string out((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return out;
}

static std::string render(const std::string & template_str, const chat_template_inputs & inputs, const chat_template_options & opts) {
  if (getenv("USE_JINJA2")) {
      return render_python(template_str, inputs);
  }
  chat_template tmpl(
      template_str,
      "",
      "");
  return tmpl.apply(inputs, opts);
}

TEST(ChatTemplateTest, SimpleCases) {
    EXPECT_THAT(render("{{ strftime_now('%Y-%m-%d %H:%M:%S') }}", {}, {}), MatchesRegex(R"([0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2})"));
}

// Regression test for check order in assistant_content:
// When both requires_non_null_content AND requires_non_empty_content are true,
// we must check non_empty first (more restrictive), otherwise null content
// would return "" which also gets rejected by templates like Mistral 3.
//
// Bug: Original code checked requires_non_null_content first, returning ""
// for null content. But Mistral 3 rejects empty strings too! The fix is to
// check requires_non_empty_content first (more restrictive).
TEST(ChatTemplateTest, RequiresNonEmptyContentCheckOrder) {
    // Template that rejects both null AND empty assistant content
    // This simulates Mistral 3's behavior via raise_exception
    const char* tmpl_str = R"(
{%- for message in messages -%}
{%- if message.role == 'assistant' -%}
{%- if message.content is none or message.content == '' -%}
{{ raise_exception('content required') }}
{%- else -%}
[ASSISTANT]{{ message.content }}[/ASSISTANT]
{%- endif -%}
{%- elif message.role == 'user' -%}
[USER]{{ message.content }}[/USER]
{%- endif -%}
{%- endfor -%}
)";

    chat_template tmpl(tmpl_str, "", "");
    auto caps = tmpl.original_caps();

    // Both flags should be true for this template
    EXPECT_TRUE(caps.requires_non_null_content);
    EXPECT_TRUE(caps.requires_non_empty_content);

    // When applying with a tool call (null content), the polyfill should
    // substitute a non-empty placeholder. With the bug (wrong check order),
    // it would substitute "" which would cause raise_exception.
    chat_template_inputs inputs;
    inputs.messages = json::array({
        {{"role", "user"}, {"content", "Hello"}},
        {{"role", "assistant"}, {"content", nullptr}, {"tool_calls", json::array({
            {{"id", "1"}, {"type", "function"}, {"function", {{"name", "test"}, {"arguments", "{}"}}}}
        })}},
    });

    // This should NOT throw - the fix ensures we substitute a non-empty
    // placeholder when requires_non_empty_content is true
    std::string result;
    EXPECT_NO_THROW(result = tmpl.apply(inputs, {}));
    EXPECT_THAT(result, HasSubstr("[ASSISTANT]"));
}
