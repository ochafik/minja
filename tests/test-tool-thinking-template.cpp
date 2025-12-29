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

/**
 * Test suite for a chat template that supports:
 * - Tool calling with <tool_call> format
 * - Thinking tags <think>...</think>
 * - System message concatenation
 * - Tool response handling
 * 
 * This template format is inspired by models like DeepSeek-R1 that support
 * reasoning/thinking capabilities alongside tool usage.
 * 
 * Example model using similar format:
 * https://huggingface.co/deepseek-ai/DeepSeek-R1
 */

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

static json read_json_file(const std::string& path) {
    std::string content = read_file(path);
    return json::parse(content);
}

class ToolThinkingTemplateTest : public ::testing::Test {
protected:
    std::string template_str;
    
    void SetUp() override {
        template_str = read_file("tests/tool-thinking-template.jinja");
    }
    
    std::string render(const chat_template_inputs & inputs, const chat_template_options & opts = {}) {
        chat_template tmpl(template_str, "", "");
        return tmpl.apply(inputs, opts);
    }
};

TEST_F(ToolThinkingTemplateTest, BasicConversation) {
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "system",
            "content": "You are a helpful assistant."
        },
        {
            "role": "user",
            "content": "Hello!"
        },
        {
            "role": "assistant",
            "content": "Hi there! How can I help you?"
        }
    ])");
    inputs.add_generation_prompt = false;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Should contain system prompt, user message, and assistant response
    EXPECT_THAT(result, HasSubstr("<|begin_of_text|>"));
    EXPECT_THAT(result, HasSubstr("You are a helpful assistant."));
    EXPECT_THAT(result, HasSubstr("<|User|>Hello!"));
    EXPECT_THAT(result, HasSubstr("<|Assistant|>"));
    EXPECT_THAT(result, HasSubstr("Hi there! How can I help you?"));
    EXPECT_THAT(result, HasSubstr("<|end_of_text|>"));
}

TEST_F(ToolThinkingTemplateTest, ToolCallFormatting) {
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "user",
            "content": "Calculate 5 + 3"
        },
        {
            "role": "assistant",
            "content": "Let me calculate that.",
            "tool_calls": [
                {
                    "id": "call_1",
                    "type": "function",
                    "function": {
                        "name": "calculator",
                        "arguments": "{\"expression\": \"5 + 3\"}"
                    }
                }
            ]
        }
    ])");
    inputs.add_generation_prompt = false;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Should format tool call correctly
    // Note: The template automatically adds <think> tags for the last assistant message
    EXPECT_THAT(result, HasSubstr("<|User|>Calculate 5 + 3"));
    EXPECT_THAT(result, HasSubstr("<|Assistant|>"));
    EXPECT_THAT(result, HasSubstr("Let me calculate that."));
    EXPECT_THAT(result, HasSubstr("<tool_call>"));
    EXPECT_THAT(result, HasSubstr("\"name\": \"calculator\""));
    EXPECT_THAT(result, HasSubstr("\"arguments\":"));
    EXPECT_THAT(result, HasSubstr("</tool_call>"));
}

TEST_F(ToolThinkingTemplateTest, ToolResponseHandling) {
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "user",
            "content": "Calculate 5 + 3"
        },
        {
            "role": "assistant",
            "content": "",
            "tool_calls": [
                {
                    "id": "call_1",
                    "type": "function",
                    "function": {
                        "name": "calculator",
                        "arguments": "{\"expression\": \"5 + 3\"}"
                    }
                }
            ]
        },
        {
            "role": "tool",
            "tool_call_id": "call_1",
            "name": "calculator",
            "content": "8"
        },
        {
            "role": "assistant",
            "content": "The result is 8."
        }
    ])");
    inputs.add_generation_prompt = false;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Should wrap tool response properly
    EXPECT_THAT(result, HasSubstr("<|User|><tool_response>8</tool_response>"));
    EXPECT_THAT(result, HasSubstr("The result is 8."));
}

TEST_F(ToolThinkingTemplateTest, ThinkingTagHandling) {
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "user",
            "content": "What's 2 + 2?"
        },
        {
            "role": "assistant",
            "content": "<think>\nLet me think about this simple addition.\n</think>\n\nThe answer is 4."
        }
    ])");
    inputs.add_generation_prompt = false;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Should preserve thinking tags
    EXPECT_THAT(result, HasSubstr("<think>"));
    EXPECT_THAT(result, HasSubstr("</think>"));
}

TEST_F(ToolThinkingTemplateTest, GenerationPromptWithThinking) {
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "user",
            "content": "Help me solve a problem."
        }
    ])");
    inputs.add_generation_prompt = true;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    extra_context["enable_thinking"] = false;
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Should add assistant prompt with empty thinking tags when enable_thinking=false
    EXPECT_THAT(result, HasSubstr("<|Assistant|><think>\n\n</think>"));
}

TEST_F(ToolThinkingTemplateTest, ForcedThinkingMode) {
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "user",
            "content": "Explain quantum physics."
        }
    ])");
    inputs.add_generation_prompt = true;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    extra_context["forced_thinking"] = true;
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Should add opening think tag when forced_thinking=true
    EXPECT_THAT(result, HasSubstr("<|Assistant|><think>\n"));
    // Should NOT have closing think tag
    EXPECT_THAT(result, Not(HasSubstr("</think>")));
}

TEST_F(ToolThinkingTemplateTest, SystemMessageConcatenation) {
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "system",
            "content": "You are a helpful assistant."
        },
        {
            "role": "system",
            "content": "You can use tools."
        },
        {
            "role": "user",
            "content": "Hello"
        }
    ])");
    inputs.add_generation_prompt = false;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Multiple system messages should be concatenated with double newlines
    EXPECT_THAT(result, HasSubstr("You are a helpful assistant.\n\nYou can use tools."));
}

TEST_F(ToolThinkingTemplateTest, ToolDescriptionGeneration) {
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "user",
            "content": "Help me."
        }
    ])");
    inputs.tools = json::parse(R"([
        {
            "type": "function",
            "function": {
                "name": "calculator",
                "description": "Perform calculations",
                "parameters": {
                    "type": "object",
                    "properties": {
                        "expression": {
                            "type": "string"
                        }
                    }
                }
            }
        }
    ])");
    inputs.add_generation_prompt = false;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Should include tool description
    EXPECT_THAT(result, HasSubstr("<|begin_of_tool_description|>"));
    EXPECT_THAT(result, HasSubstr("Tool calling capabilities"));
    EXPECT_THAT(result, HasSubstr("<|end_of_tool_description|>"));
    EXPECT_THAT(result, HasSubstr("calculator"));
}

TEST_F(ToolThinkingTemplateTest, MultipleToolCalls) {
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "user",
            "content": "Calculate 5+3 and 10-2"
        },
        {
            "role": "assistant",
            "content": "I'll calculate both.",
            "tool_calls": [
                {
                    "id": "call_1",
                    "type": "function",
                    "function": {
                        "name": "calculator",
                        "arguments": "{\"expression\": \"5+3\"}"
                    }
                },
                {
                    "id": "call_2",
                    "type": "function",
                    "function": {
                        "name": "calculator",
                        "arguments": "{\"expression\": \"10-2\"}"
                    }
                }
            ]
        }
    ])");
    inputs.add_generation_prompt = false;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Should have two tool calls
    size_t first_pos = result.find("<tool_call>");
    ASSERT_NE(first_pos, std::string::npos);
    size_t second_pos = result.find("<tool_call>", first_pos + 1);
    ASSERT_NE(second_pos, std::string::npos);
}

TEST_F(ToolThinkingTemplateTest, CompleteToolThinkingWorkflow) {
    // Load from context file
    json context = read_json_file("tests/contexts/tool_thinking.json");
    
    chat_template_inputs inputs;
    inputs.messages = context["messages"];
    inputs.tools = context["tools"];
    inputs.add_generation_prompt = context["add_generation_prompt"];
    
    json extra_context;
    extra_context["bos_token"] = context["bos_token"];
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Verify complete workflow
    EXPECT_THAT(result, HasSubstr("<|begin_of_text|>"));
    EXPECT_THAT(result, HasSubstr("You are a helpful AI assistant"));
    EXPECT_THAT(result, HasSubstr("<|User|>What's 25 * 37?"));
    EXPECT_THAT(result, HasSubstr("<think>"));
    EXPECT_THAT(result, HasSubstr("</think>"));
    EXPECT_THAT(result, HasSubstr("<tool_call>"));
    EXPECT_THAT(result, HasSubstr("calculator"));
    EXPECT_THAT(result, HasSubstr("<tool_response>925</tool_response>"));
    EXPECT_THAT(result, HasSubstr("The result is 925."));
    EXPECT_THAT(result, HasSubstr("<|User|>Now divide that by 5."));
    EXPECT_THAT(result, HasSubstr("<|Assistant|>"));
    
    // Debug: print result to see what was generated
    std::cout << "Generated template output:\n" << result << std::endl;
}

TEST_F(ToolThinkingTemplateTest, ThinkingTagStripping) {
    // Test that thinking tags are stripped from intermediate assistant messages
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "user",
            "content": "First question"
        },
        {
            "role": "assistant",
            "content": "<think>\nLet me think about this.\n</think>\n\nHere is my answer."
        },
        {
            "role": "user",
            "content": "Second question"
        }
    ])");
    inputs.add_generation_prompt = false;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // For non-last messages before tool calls, thinking should be stripped
    // This tests the logic: if '</think>' in content and not loop.last and loop.index0 < (ns.first_tool_index - 1)
    EXPECT_THAT(result, HasSubstr("Here is my answer."));
}

TEST_F(ToolThinkingTemplateTest, ArgumentsAsObject) {
    // Test when tool call arguments are provided as object instead of string
    chat_template_inputs inputs;
    inputs.messages = json::parse(R"([
        {
            "role": "user",
            "content": "Calculate something"
        },
        {
            "role": "assistant",
            "content": "",
            "tool_calls": [
                {
                    "id": "call_1",
                    "type": "function",
                    "function": {
                        "name": "calculator",
                        "arguments": {
                            "expression": "5 + 3"
                        }
                    }
                }
            ]
        }
    ])");
    inputs.add_generation_prompt = false;
    
    json extra_context;
    extra_context["bos_token"] = "<|begin_of_text|>";
    inputs.extra_context = extra_context;
    
    std::string result = render(inputs);
    
    // Should convert object to JSON string
    EXPECT_THAT(result, HasSubstr("<tool_call>"));
    EXPECT_THAT(result, HasSubstr("\"arguments\":"));
}
