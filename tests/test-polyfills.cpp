/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include "minja.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <fstream>
#include <iostream>
#include <string>
#include "chat-template.hpp"

using namespace minja;

#define TEMPLATE_CHATML \
    "{%- for message in messages -%}\n" \
    "  {{- '<|im_start|>' + message.role + '\n' + message.content + '<|im_end|>\n' -}}\n" \
    "{%- endfor -%}\n" \
    "{%- if add_generation_prompt -%}\n" \
    "  {{- '<|im_start|>assistant\n' -}}\n" \
    "{%- endif -%}"


#define TEMPLATE_CHATML_NO_SYSTEM \
    "{%- for message in messages -%}\n" \
    "  {%- if message.role == 'system' -%}\n" \
    "    {{- raise_exception('System role not supported') -}}\n" \
    "  {%- endif -%}\n" \
    "  {{- '<|im_start|>' + message.role + '\n' + message.content + '<|im_end|>\n' -}}\n" \
    "{%- endfor -%}\n" \
    "{%- if add_generation_prompt -%}\n" \
    "  {{- '<|im_start|>assistant\n' -}}\n" \
    "{%- endif -%}"


#define TEMPLATE_DUMMY \
    "{%- for tool in tools -%}\n" \
    "  {{- 'tool: ' + (tool | tojson(indent=2)) + '\n'  -}}\n" \
    "{%- endfor -%}\n" \
    "{%- for message in messages -%}\n" \
    "  {{- 'message: ' + (message | tojson(indent=2)) + '\n' -}}\n" \
    "{%- endfor -%}\n" \
    "{%- if add_generation_prompt -%}\n" \
    "  {{- 'message: ' -}}\n" \
    "{%- endif -%}"


const json message_user_text {
    { "role",    "user"     },
    { "content", "I need help" },
};
const json message_assistant_text {
    { "role",    "assistant"     },
    { "content", "Hello, world!" },
};
const json message_system {
    { "role",    "system"     },
    { "content", "I am The System!" },
};
const json tool_calls = json::array({{
    { "type", "function" },
    { "function", { { "name", "special_function" }, { "arguments", "{\"arg1\": 1}" } } },
}});

const json message_assistant_call {
    { "role",       "assistant"},
    { "content",    {}},
    { "tool_calls", {
        {
            { "type", "function" },
            { "function", {
                { "name", "special_function" },
                { "arguments", "{\"arg1\": 1}" },
            }},
        },
    }},
};
const json message_assistant_call_id {
    { "role",       "assistant"},
    { "content",    {}},
    { "tool_calls", {
        {
            { "type", "function" },
            { "function", {
                { "name", "special_function" },
                { "arguments", "{\"arg1\": 1}" },
            }},
            {"id", "123456789"},
        },
    }},
    { "role",       "assistant"                },
    { "content",    {}                         },
    { "tool_calls", tool_calls                  }
};
const json message_assistant_call_idx {
    { "role",       "assistant"},
    { "content",    {}},
    { "tool_plan",  "I'm not so sure"},
    { "tool_calls", {
        {
            { "type", "function" },
            { "function", {
                { "name", "special_function" },
                { "arguments", "{\"arg1\": 1}" },
            }},
            {"id", "0"},
        },
    }},
    { "role",       "assistant"                },
    { "content",    {}                         },
    { "tool_calls", tool_calls                  }
};
const json message_tool {
    { "role",       "tool"},
    { "content",    {
      {"result", 123},
    }},
};

const auto special_function_tool = json::parse(R"({
  "type": "function",
  "function": {
    "name": "special_function",
    "description": "I'm special",
    "parameters": {
      "type": "object",
      "properties": {
        "arg1": {
          "type": "integer",
          "description": "The arg."
        }
      },
      "required": ["arg1"]
    }
  }
})");

auto ThrowsWithSubstr = [](const std::string & expected_substr) {
    return testing::Throws<std::runtime_error>(Property(&std::runtime_error::what, testing::HasSubstr(expected_substr)));
};

static chat_template_options options_no_polyfills() {
    chat_template_options opts;
    opts.apply_polyfills = false;
    opts.polyfill_system_role = false;
    opts.polyfill_tools = false;
    opts.polyfill_tool_call_examples = false;
    opts.polyfill_tool_calls = false;
    opts.polyfill_tool_responses = false;
    opts.polyfill_object_arguments = false;
    opts.polyfill_typed_content = false;
    return opts;
};

TEST(PolyfillTest, NoPolyFill) {
    chat_template tmpl(TEMPLATE_CHATML, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_user_text});

    EXPECT_EQ(
        "<|im_start|>user\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n",
        tmpl.apply(inputs, options_no_polyfills()));

    inputs.add_generation_prompt = false;
    EXPECT_EQ(
        "<|im_start|>user\n"
        "I need help<|im_end|>\n",
        tmpl.apply(inputs, options_no_polyfills()));

    inputs.messages = json::array({message_user_text, message_assistant_text});
    EXPECT_EQ(
        "<|im_start|>user\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n"
        "Hello, world!<|im_end|>\n",
        tmpl.apply(inputs, options_no_polyfills()));
}

TEST(PolyfillTest, SystemRoleSupported) {
    chat_template chatml(TEMPLATE_CHATML, "", "");
    chat_template dummy(TEMPLATE_DUMMY, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_system, message_user_text});

    EXPECT_EQ(
        "<|im_start|>system\n"
        "I am The System!<|im_end|>\n"
        "<|im_start|>user\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n",
        chatml.apply(inputs));
    EXPECT_EQ(
        "message: {\n"
        "  \"role\": \"system\",\n"
        "  \"content\": \"I am The System!\"\n"
        "}\n"
        "message: {\n"
        "  \"role\": \"user\",\n"
        "  \"content\": \"I need help\"\n"
        "}\n"
        "message: ",
        dummy.apply(inputs));
}

TEST(PolyfillTest, SystemRolePolyfill) {
    chat_template tmpl(TEMPLATE_CHATML_NO_SYSTEM, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_system, message_user_text});

    EXPECT_THAT(
        [&]() { tmpl.apply(inputs, options_no_polyfills()); },
        ThrowsWithSubstr("System role not supported"));

    EXPECT_EQ(
        "<|im_start|>user\n"
        "I am The System!\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n",
        tmpl.apply(inputs));
}

TEST(PolyfillTest, ToolCallSupported) {
    chat_template tmpl(TEMPLATE_DUMMY, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_user_text, message_assistant_call_id});

    EXPECT_EQ(
        "message: {\n"
        "  \"role\": \"user\",\n"
        "  \"content\": \"I need help\"\n"
        "}\n"
        "message: {\n"
        "  \"role\": \"assistant\",\n"
        "  \"content\": null,\n"
        "  \"tool_calls\": [\n"
        "    {\n"
        "      \"type\": \"function\",\n"
        "      \"function\": {\n"
        "        \"name\": \"special_function\",\n"
        "        \"arguments\": {\n"
        "          \"arg1\": 1\n"
        "        }\n"
        "      },\n"
        "      \"id\": \"123456789\"\n"
        "    }\n"
        "  ]\n"
        "}\n"
        "message: ",
        tmpl.apply(inputs));
}

TEST(PolyfillTest, ToolCallPolyfill) {
    chat_template tmpl(TEMPLATE_CHATML, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_user_text, message_assistant_call_id});

    EXPECT_EQ(
        "<|im_start|>user\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n"
        "{\n"
        "  \"tool_calls\": [\n"
        "    {\n"
        "      \"name\": \"special_function\",\n"
        "      \"arguments\": {\n"
        "        \"arg1\": 1\n"
        "      },\n"
        "      \"id\": \"123456789\"\n"
        "    }\n"
        "  ]\n"
        "}<|im_end|>\n"
        "<|im_start|>assistant\n",
        tmpl.apply(inputs));
}

TEST(PolyfillTest, ToolsPolyfill) {
    chat_template tmpl(TEMPLATE_CHATML, "", "<|im_end|>");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_user_text});
    inputs.tools = json::array({special_function_tool});

    EXPECT_EQ(
        "<|im_start|>system\n"
        "You can call any of the following tools to satisfy the user's requests: [\n"
        "  {\n"
        "    \"type\": \"function\",\n"
        "    \"function\": {\n"
        "      \"name\": \"special_function\",\n"
        "      \"description\": \"I'm special\",\n"
        "      \"parameters\": {\n"
        "        \"type\": \"object\",\n"
        "        \"properties\": {\n"
        "          \"arg1\": {\n"
        "            \"type\": \"integer\",\n"
        "            \"description\": \"The arg.\"\n"
        "          }\n"
        "        },\n"
        "        \"required\": [\n"
        "          \"arg1\"\n"
        "        ]\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "]\n"
        "\n"
        "Example tool call syntax:\n"
        "\n"
        "{\n"
        "  \"tool_calls\": [\n"
        "    {\n"
        "      \"name\": \"tool_name\",\n"
        "      \"arguments\": {\n"
        "        \"arg1\": \"some_value\"\n"
        "      },\n"
        "      \"id\": \"call_1___\"\n"
        "    }\n"
        "  ]\n"
        "}\n\n<|im_end|>\n"
        "<|im_start|>user\n"
        "I need help<|im_end|>\n"
        "<|im_start|>assistant\n",
        tmpl.apply(inputs));
}

TEST(PolyfillTest, ToolSupported) {
    chat_template tmpl(TEMPLATE_DUMMY, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "message: {\n"
        "  \"role\": \"tool\",\n"
        "  \"content\": {\n"
        "    \"result\": 123\n"
        "  }\n"
        "}\n"
        "message: ",
        tmpl.apply(inputs));
}

TEST(PolyfillTest, ToolPolyfill) {
    chat_template tmpl(TEMPLATE_CHATML_NO_SYSTEM, "", "");

    auto inputs = chat_template_inputs();
    inputs.messages = json::array({message_tool});

    EXPECT_EQ(
        "<|im_start|>user\n{\n"
        "  \"tool_response\": {\n"
        "    \"content\": {\n"
        "      \"result\": 123\n"
        "    }\n"
        "  }\n"
        "}<|im_end|>\n"
        "<|im_start|>assistant\n",
        tmpl.apply(inputs));
}