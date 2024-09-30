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

#undef NDEBUG
#include <cassert>

#include <iostream>
#include <fstream>
#include <iostream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

using json = nlohmann::ordered_json;

static std::string filename_without_extension(const std::string &path)
{
    auto res = path;
    auto pos = res.find_last_of('/');
    if (pos != std::string::npos)
        res = res.substr(pos + 1);
    pos = res.find_last_of('.');
    if (pos != std::string::npos)
        res = res.substr(0, pos);
    return res;
}

template <class T>
static void assert_equals(const T &expected, const T &actual)
{
    if (expected != actual)
    {
        std::cerr << "Expected: " << expected << std::endl;
        std::cerr << "Actual: " << actual << std::endl;
        std::cerr << std::flush;
        throw std::runtime_error("Test failed");
    }
}

static std::vector<std::string> find_files(const std::string &folder, const std::string &ext)
{
    std::vector<std::string> files;
    // Note: once we can use C++17 this becomes:
    //   for (const auto & entry : std::filesystem::directory_iterator(folder))
    //     if (entry.path().extension() == ext) files.push_back(entry.path().string());
#ifdef _WIN32
    std::string search_path = folder + "\\*" + ext;
    WIN32_FIND_DATA fd;
    HANDLE hFind = ::FindFirstFile(search_path.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                files.push_back(folder + "\\" + fd.cFileName);
            }
        } while (::FindNextFile(hFind, &fd));
        ::FindClose(hFind);
    }
#else
    DIR *dir = opendir(folder.c_str());
    if (dir != nullptr)
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_type == DT_REG)
            {
                std::string filename = entry->d_name;
                if (filename.length() >= ext.length() &&
                    filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0)
                {
                    files.push_back(folder + "/" + filename);
                }
            }
        }
        closedir(dir);
    }
#endif
    return files;
}

static std::string read_file(const std::string &path)
{
    std::ifstream fs(path, std::ios_base::binary);
    if (!fs.is_open())
    {
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

int main(void)
{
    auto jinja_template_files = find_files("third_party/templates", ".jinja");
    auto context_files = find_files("tests/contexts", ".json");

    auto get_golden_file = [&](const std::string &tmpl_file, const std::string &ctx_file)
    {
        auto tmpl_name = filename_without_extension(tmpl_file);
        auto ctx_name = filename_without_extension(ctx_file);
        auto golden_name = tmpl_name + "-" + ctx_name;
        return "tests/goldens/" + golden_name + ".txt";
    };
    auto fail_with_golden_instructions = [&]()
    {
        throw std::runtime_error("To fetch templates and generate golden files, run `python tests/update_chat_template_goldens.py`");
    };

    if (jinja_template_files.empty())
    {
        std::cerr << "No Jinja templates found in third_party/templates" << std::endl;
        fail_with_golden_instructions();
    }

    for (const auto &tmpl_file : jinja_template_files)
    {
        std::cout << "# Testing template: " << tmpl_file << std::endl
                  << std::flush;
        auto tmpl_str = read_file(tmpl_file);

        auto found_goldens = false;

        for (const auto &ctx_file : context_files)
        {
            auto ctx = json::parse(read_file(ctx_file));

            minja::chat_template tmpl(
                tmpl_str,
                ctx.at("bos_token"),
                ctx.at("eos_token"));

            auto golden_file = get_golden_file(tmpl_file, ctx_file);
            std::string expected;
            try
            {
                expected = read_file(golden_file);
            }
            catch (const std::runtime_error &e)
            {
                // No golden file.
                continue;
            }
            found_goldens = true;
            std::cout << "  - " << golden_file << std::endl
                      << std::flush;

            std::string actual;
            try
            {
                actual = tmpl.apply(
                    ctx.at("messages"),
                    ctx.contains("tools") ? ctx.at("tools") : json(),
                    ctx.at("add_generation_prompt"),
                    ctx.contains("tools") ? json{
                                                {"builtin_tools", {"wolfram_alpha", "brave_search"}}}
                                          : json());
            }
            catch (const std::runtime_error &e)
            {
                actual = "ERROR: " + std::string(e.what());
            }
            assert_equals(expected, actual);
        }

        if (!found_goldens)
        {
            std::cerr << "No golden files found for " << tmpl_file << std::endl;
            fail_with_golden_instructions();
        }
    }
    return 0;
}
