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
        std::cerr << "Expected: " << expected << std::endl;
        std::cerr << "Actual: " << actual << std::endl;
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

int main(int argc, char *argv[]) {
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " <template_file.jinja> <context_file.json> <golden_file.txt>" << std::endl;
        for (int i = 0; i < argc; i++)
        {
            std::cerr << "argv[" << i << "] = " << argv[i] << std::endl;
        }
        return 1;
    }

    try {
        std::string tmpl_file = argv[1];
        std::string ctx_file = argv[2];
        std::string golden_file = argv[3];
        
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
