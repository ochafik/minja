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

  private:
    bool supports_tools_ = true;
    // Meta-Llama-3.1-8B-Instruct's template expects arguments to be an object.
    // Most other templates (and OpenAI's API) expect the arguments object to be stringified.
    bool requires_object_arguments_ = false;
    bool supports_system_role_ = true;
    bool supports_parallel_tool_calls_ = false;
    std::string source_;
    std::string bos_token_;
    std::string eos_token_;
    std::shared_ptr<minja::TemplateNode> template_root_;

    bool renders_needles(
        const std::vector<std::string> & needles,
        const nlohmann::ordered_json & messages,
        const nlohmann::ordered_json & tools,
        bool add_generation_prompt,
        const nlohmann::ordered_json & extra_context = nlohmann::ordered_json()) const
    {
        try {
            auto prompt = apply(messages, tools, add_generation_prompt, extra_context);
            for (const auto & needle : needles) {
                if (prompt.find(needle) == std::string::npos) {
                    return false;
                }
            }
            return true;
        } catch (const std::exception & e) {
            return false;
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
        supports_tools_ = source.find("tools") != std::string::npos;
        requires_object_arguments_ =
            source.find("tool_call.arguments | items") != std::string::npos
            || source.find("tool_call.arguments | tojson") != std::string::npos;
        supports_parallel_tool_calls_ = source.find("tool_call_id") != std::string::npos;

        supports_system_role_ = renders_needles({"<System Needle>"}, {
            {{"role", "system"}, {"content", "<System Needle>"}},
            {{"role", "user"},   {"content", "Hey"}}
        }, {}, false);
    }

    const std::string & source() const { return source_; }
    bool supports_tools() const { return supports_tools_; }
    bool supports_parallel_tool_calls() const { return supports_parallel_tool_calls_; }

    std::string apply(
        const nlohmann::ordered_json & messages,
        const nlohmann::ordered_json & tools,
        bool add_generation_prompt,
        const nlohmann::ordered_json & extra_context = nlohmann::ordered_json()) const
    {
        json actual_messages;

        // First, "fix" messages so they have a chance to be rendered correctly by the template

        if (requires_object_arguments_ || !supports_system_role_ || !supports_tools_) {
            actual_messages = json::array();

            std::string pending_system;
            auto flush_sys = [&]() {
                if (!pending_system.empty()) {
                    actual_messages.push_back({
                        {"role", "user"},
                        {"content", pending_system},
                    });
                    pending_system.clear();
                }
            };
            for (const auto & message_ : messages) {
                auto message = message_;
                if (!message.contains("role") || !message.contains("content")) {
                    throw std::runtime_error("message must have 'role' and 'content' fields: " + message.dump());
                }
                std::string role = message.at("role");

                if (message.contains("tool_calls")) {
                    if (requires_object_arguments_ || !supports_tools_) {
                        for (auto & tool_call : message.at("tool_calls")) {
                            if (tool_call["type"] == "function") {
                                auto & function = tool_call.at("function");
                                std::string arguments = function.at("arguments");
                                function["arguments"] = json::parse(arguments);
                            }
                        }
                    }
                    if (!supports_tools_) {
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
                if (!supports_tools_ && role == "tool") {
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

                // std::string content = message["content"];
                if (!message["content"].is_null() && !supports_system_role_) {
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
                actual_messages.push_back(message);
            }
            flush_sys();
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
};

}  // namespace minja
