/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#pragma once

#include "minja.hpp"
#include <json.hpp>
#include <string>
#include <vector>

using json = nlohmann::ordered_json;

namespace minja {

class chat_template {
  public:
      struct chat_template_caps {
        bool supports_tools = true;
        bool supports_tool_calls = true;
        bool supports_tool_responses = true;
        bool supports_system_role = true;
        bool supports_parallel_tool_calls = false;
        // meta-llama/Llama-3.1-8B-Instruct expects arguments to be an object.
        // Most other templates (and OpenAI's API) expect the arguments object to be stringified.
        bool requires_object_arguments = false;
        // MiniMaxAI/MiniMax-Text-01 special
        bool requires_typed_content = false;
    };

  private:
    chat_template_caps caps_;
    std::string source_;
    std::string bos_token_;
    std::string eos_token_;
    std::shared_ptr<minja::TemplateNode> template_root_;

    std::string try_raw_render(
        const nlohmann::ordered_json & messages,
        const nlohmann::ordered_json & tools,
        bool add_generation_prompt,
        const nlohmann::ordered_json & extra_context = nlohmann::ordered_json()) const
    {
        try {
            auto prompt = apply(messages, tools, add_generation_prompt, extra_context, /* adjust_inputs= */ false);
// #ifndef NDEBUG
//             fprintf(stderr, "try_raw_render: %s\n", prompt.c_str());
// #endif
            return prompt;
        } catch (const std::exception & e) {
#ifndef NDEBUG
            fprintf(stderr, "try_raw_render error: %s\n", e.what());
#endif
            return "";
        }
    }

  public:

    chat_template(const std::string & source, const std::string & bos_token, const std::string & eos_token)
        : source_(source), bos_token_(bos_token), eos_token_(eos_token)
    {
        template_root_ = minja::Parser::parse(source_, {
            /* .trim_blocks = */ true,
            /* .lstrip_blocks = */ true,
            /* .keep_trailing_newline = */ false,
        });

        auto contains = [](const std::string & haystack, const std::string & needle) {
            return haystack.find(needle) != std::string::npos;
        };

        const json dummy_str_user_msg = {{"role", "user"}, {"content", "Hey"}};
        const json dummy_typed_user_msg = {{"role", "user"}, {"content", json::array({{{"type", "text"}, {"text", "Hey"}}})}};

        caps_.requires_typed_content =
            !contains(try_raw_render(json::array({dummy_str_user_msg}), {}, false), "Hey")
            && contains(try_raw_render(json::array({dummy_typed_user_msg}), {}, false), "Hey");

        const auto dummy_user_msg = caps_.requires_typed_content
            ? dummy_typed_user_msg
            : dummy_str_user_msg;
        const std::string needle = "<System Needle>";
        const json needle_system_msg = {
            {"role", "system"},
            {"content", caps_.requires_typed_content ? json::array({{{"type", "text"}, {"text", needle}}}) : json(needle)},
        };

        const json dummy_tool_call_obj_args {
            {"id", "call_1___"},
            {"type", "function"},
            {"function", {
                {"arguments", {
                    {"code", "print('Hello, World!')"},
                }},
                {"name", "ipython"},
            }},
        };
        const json dummy_tool_call_str_args {
            {"id", "call_1___"},
            {"type", "function"},
            {"function", {
                {"arguments", "{\"code\": \"print('Hello, World!')\"}"},
                {"name", "ipython"},
            }},
        };

        caps_.supports_parallel_tool_calls = contains(source, "tool_call_id");
        caps_.supports_tool_calls = contains(source, "tool_calls");
        caps_.supports_system_role = contains(try_raw_render({needle_system_msg, dummy_user_msg,}, {}, false), needle);

        caps_.supports_tools =
            contains(try_raw_render(json::array({
                dummy_user_msg
            }), json::array({
                {
                    {"type", "function"},
                    {"function", {
                        {"name", "some_tool"},
                        {"parameters", {
                            {"type", "object"},
                            {"properties", {
                                {"arg", "string"},
                            }},
                            {"required", json::array({ "arg" })},
                        }},
                    }},
                },
            }), false), "some_tool");

        caps_.requires_object_arguments =
            contains(try_raw_render(json::array({
                dummy_user_msg,
                {
                    {"role", "assistant"},
                    {"tool_calls", json::array({dummy_tool_call_obj_args})},
                }
            }), {}, false), "{\"code\": \"print")
            && !contains(try_raw_render(json::array({
                dummy_user_msg,
                {
                    {"role", "assistant"},
                    {"tool_calls", json::array({dummy_tool_call_str_args})},
                }
            }), {}, false), "{\"code\": \"print");
        auto dummy_tool_call = caps_.requires_object_arguments ? dummy_tool_call_obj_args : dummy_tool_call_str_args;

        caps_.supports_tool_responses =
            contains(try_raw_render(json::array({
                dummy_user_msg,
                {
                    {"role", "assistant"},
                    {"tool_calls", json::array({dummy_tool_call})},
                },
                {
                    {"role", "tool"},
                    {"name", "some_tool"},
                    {"content", "Some response!"},
                    {"tool_call_id", "call_1___"},
                }
            }), {}, false), "Some response!");
    }

    const std::string & source() const { return source_; }
    const std::string & bos_token() const { return bos_token_; }
    const std::string & eos_token() const { return eos_token_; }
    const chat_template_caps & original_caps() const { return caps_; }

    std::string apply(
        const nlohmann::ordered_json & messages,
        const nlohmann::ordered_json & tools,
        bool add_generation_prompt,
        const nlohmann::ordered_json & extra_context = nlohmann::ordered_json(),
        bool adjust_inputs = true) const
    {
        json actual_messages;

        auto needs_adjustments = adjust_inputs && (false
            || !caps_.supports_system_role
            || !caps_.supports_tools
            || !caps_.supports_tool_responses
            || !caps_.supports_tool_calls
            || caps_.requires_object_arguments
            || caps_.requires_typed_content
        );
        if (needs_adjustments) {
            actual_messages = json::array();

            auto add_message = [&](const json & msg) {
                if (caps_.requires_typed_content && msg.contains("content") && !msg.at("content").is_null() && msg.at("content").is_string()) {
                    actual_messages.push_back({
                        {"role", msg.at("role")},
                        {"content", {{
                            {"type", "text"},
                            {"text", msg.at("content")},
                        }}},
                    });
                } else {
                    actual_messages.push_back(msg);
                }
            };

            std::string pending_system;
            auto flush_sys = [&]() {
                if (!pending_system.empty()) {
                    add_message({
                        {"role", "user"},
                        {"content", pending_system},
                    });
                    pending_system.clear();
                }
            };
            auto needs_tools_in_system = !tools.is_null() && tools.size() > 0 && !caps_.supports_tools;

            for (const auto & message_ : needs_tools_in_system ? add_system(messages, "Available tools: " + tools.dump(2)) : messages) {
                auto message = message_;
                if (!message.contains("role") || !message.contains("content")) {
                    throw std::runtime_error("message must have 'role' and 'content' fields: " + message.dump());
                }
                std::string role = message.at("role");

                if (message.contains("tool_calls")) {
                    if (caps_.requires_object_arguments || !caps_.supports_tool_calls) {
                        for (auto & tool_call : message.at("tool_calls")) {
                            if (tool_call["type"] == "function") {
                                auto & function = tool_call.at("function");
                                auto & arguments = function.at("arguments");
                                if (arguments.is_string()) {
                                    try {
                                        arguments = json::parse(arguments.get<std::string>());
                                    } catch (const std::exception & ecvt) {
                                        fprintf(stderr, "Failed to parse arguments: %s\n", ecvt.what());
                                    }
                                }
                            }
                        }
                    }
                    if (!caps_.supports_tool_calls) {
                        auto content = message.at("content");
                        auto tool_calls = json::array();
                        for (const auto & tool_call : message.at("tool_calls")) {
                            if (tool_call.at("type") != "function") {
                                continue;
                            }
                            const auto & function = tool_call.at("function");
                            auto tc = json {
                                {"name", function.at("name")},
                                {"arguments", function.at("arguments")},
                            };
                            if (tool_call.contains("id")) {
                                tc["id"] = tool_call["id"];
                            }
                            tool_calls.push_back(tc);
                        }
                        auto obj = json {
                            {"tool_calls", tool_calls},
                        };
                        if (!content.is_null() && content != "") {
                            obj["content"] = content;
                        }
                        message["content"] = obj.dump(2);
                        message.erase("tool_calls");
                    }
                }
                if (!caps_.supports_tool_responses && role == "tool") {
                    message["role"] = "user";
                    auto obj = json {
                        {"tool_response", {
                            {"tool", message.at("name")},
                            {"content", message.at("content")},
                        }},
                    };
                    if (message.contains("tool_call_id")) {
                        obj["tool_response"]["tool_call_id"] = message.at("tool_call_id");
                    }
                    message["content"] = obj.dump(2);
                    message.erase("name");
                }

                if (!message["content"].is_null() && !caps_.supports_system_role) {
                    std::string content = message.at("content");
                    if (role == "system") {
                        if (!pending_system.empty()) pending_system += "\n";
                        pending_system += content;
                        continue;
                    } else {
                        if (role == "user") {
                            if (!pending_system.empty()) {
                                message["content"] = pending_system + (content.empty() ? "" : "\n" + content);
                                pending_system.clear();
                            }
                        } else {
                            flush_sys();
                        }
                    }
                }
                add_message(message);
            }
            if (!caps_.supports_system_role) {
                flush_sys();
            }
        } else {
            actual_messages = messages;
        }

        auto context = minja::Context::make(json({
            {"messages", actual_messages},
            {"add_generation_prompt", add_generation_prompt},
            {"bos_token", bos_token_},
            {"eos_token", eos_token_},
        }));

        if (!tools.is_null()) {
            auto tools_val = minja::Value(tools);
            context->set("tools", tools_val);
        }
        if (!extra_context.is_null()) {
            for (auto & kv : extra_context.items()) {
                minja::Value val(kv.value());
                context->set(kv.key(), val);
            }
        }

        return template_root_->render(context);
    }

    static nlohmann::ordered_json add_system(const nlohmann::ordered_json & messages, const std::string & system_prompt) {
        json messages_with_system = messages;

        if (messages_with_system.size() > 0 && messages_with_system[0].at("role") == "system") {
            std::string existing_system = messages_with_system.at(0).at("content");
            messages_with_system[0] = json {
                {"role", "system"},
                {"content", existing_system + "\n" + system_prompt},
            };
        } else {
            messages_with_system.insert(messages_with_system.begin(), json {
                {"role", "system"},
                {"content", system_prompt},
            });
        }
        return messages_with_system;
    }
};

}  // namespace minja
