/*
    Copyright 2024 Google LLC

    Use of this source code is governed by an MIT-style
    license that can be found in the LICENSE file or at
    https://opensource.org/licenses/MIT.
*/
// SPDX-License-Identifier: MIT
#include "chat-template.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <stdexcept>

#undef NDEBUG
#include <cassert>

using json = nlohmann::ordered_json;

template <class T>
static void assert_equals(const T &expected, const T &actual){
    if (expected != actual) {
        std::cerr << "Expected: " << expected << "\n\n";
        std::cerr << "Actual: " << actual << "\n\n";
        auto i_divergence = std::min(expected.size(), actual.size());
        for (size_t i = 0; i < i_divergence; i++) {
            if (expected[i] != actual[i]) {
                i_divergence = i;
                break;
            }
        }
        std::cerr << "Divergence at index " << i_divergence << "\n\n";
        std::cerr << "Expected suffix: " << expected.substr(i_divergence) << "\n\n";
        std::cerr << "Actual suffix: " << actual.substr(i_divergence) << "\n\n";
        
        std::cerr << std::flush;
        throw std::runtime_error("Test failed");
    }
}

static std::string read_file(const std::string &path) {
    std::ifstream fs(path, std::ios_base::binary);
    if (!fs.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    fs.seekg(0, std::ios_base::end);
    auto size = fs.tellg();
    fs.seekg(0);
    std::string out;
    out.resize(static_cast<size_t>(size));
    fs.read(&out[0], static_cast<std::streamsize>(size));
    return out;
}

// static void write_file(const std::string &path, const std::string &content) {
//     std::ofstream fs(path, std::ios_base::binary);
//     if (!fs.is_open()) {
//         throw std::runtime_error("Failed to open file: " + path);
//     }
//     fs.write(content.c_str(), content.size());
// }

static json caps_to_json(const minja::chat_template::chat_template_caps &caps) {
    return {
        {"supports_tools", caps.supports_tools},
        {"supports_tool_calls", caps.supports_tool_calls},
        {"supports_tool_responses", caps.supports_tool_responses},
        {"supports_system_role", caps.supports_system_role},
        {"supports_parallel_tool_calls", caps.supports_parallel_tool_calls},
        {"supports_tool_call_id", caps.supports_tool_call_id},
        {"requires_object_arguments", caps.requires_object_arguments},
        {"requires_typed_content", caps.requires_typed_content},
    };
}

int main(int argc, char *argv[]) {
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " <template_file.jinja> <template_file.jinja.caps.json> <context_file.json> <golden_file.txt>" << std::endl;
        for (int i = 0; i < argc; i++)
        {
            std::cerr << "argv[" << i << "] = " << argv[i] << std::endl;
        }
        return 1;
    }

    try {
        std::string tmpl_file = argv[1];
        std::string caps_file = argv[2];
        std::string ctx_file = argv[3];
        std::string golden_file = argv[4];

        auto tmpl_str = read_file(tmpl_file);
        
        if (ctx_file == "n/a")
        {
            std::cout << "# Skipping template: " << tmpl_file << "\n" << tmpl_str << std::endl;
            return 127;
        }

        std::cout << "# Testing template: " << tmpl_file << std::endl
                << "# With context: " << ctx_file << std::endl
                << "# Against golden file: " << golden_file << std::endl
                << std::flush;

        auto ctx = json::parse(read_file(ctx_file));

        minja::chat_template tmpl(
            tmpl_str,
            ctx.at("bos_token"),
            ctx.at("eos_token"));

        // Checks that the Python & C++ capability detection codes are in sync.
        auto expected_caps = read_file(caps_file);
        auto caps = caps_to_json(tmpl.original_caps()).dump(2);
        assert_equals(expected_caps, caps);
        // write_file(caps_file, caps_to_json(tmpl.original_caps()).dump(2));
        // std::cout << "# Wrote caps to: " << caps_file << std::endl;

        std::string expected;
        try {
            expected = minja::normalize_newlines(read_file(golden_file));
        } catch (const std::exception &e) {
            std::cerr << "Failed to read golden file: " << golden_file << std::endl;
            std::cerr << e.what() << std::endl;
            return 1;
        }

        std::string actual;
        try {
            actual = tmpl.apply(
                ctx.at("messages"),
                ctx.contains("tools") ? ctx.at("tools") : json(),
                ctx.at("add_generation_prompt"),
                ctx.contains("tools") ? json{
                                            {"builtin_tools", {"wolfram_alpha", "brave_search"}}}
                                    : json());
        } catch (const std::exception &e) {
            std::cerr << "Error applying template: " << e.what() << std::endl;
            return 1;
        }

        assert_equals(expected, actual);
        std::cout << "Test passed successfully." << std::endl;
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
