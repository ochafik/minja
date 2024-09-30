# minja.hpp - A minimalistic C++ Jinja templating engine for LLM chat templates

_**This is not an official Google product**_

Minja is a minimalistic reimplementation of the [Jinja](https://github.com/pallets/jinja/) templating engine to integrate in/with C++ LLM projects (such as [llama.cpp](https://github.com/ggerganov/llama.cpp) or [gemma.cpp](https://github.com/google/gemma.cpp)).

It is **not general purpose**: it includes just what’s needed for actual chat templates (very limited set of filters, tests and language features). Users with different needs should look at third-party alternatives such as [Jinja2Cpp](https://github.com/jinja2cpp/Jinja2Cpp), [Jinja2CppLight](https://github.com/hughperkins/Jinja2CppLight), or [inja](https://github.com/pantor/inja) (none of which we endorse).

## Design goals:

- Support each and every major LLM found on HuggingFace
    - See [update_templates_and_goldens.py](./update_templates_and_goldens.py) and [third_party/templates](./third_party/templates) for the list of models currently supported
- Keep codebase small (currently 2.5k LoC) and easy to maintain / extend
- Easy to integrate to projects such as [llama.cpp](https://github.com/ggerganov/llama.cpp) or [gemma.cpp](https://github.com/google/gemma.cpp):
  - Header-only
  - C++11
  - Only depend on [nlohmann::json](https://github.com/nlohmann/json) (no Boost)
- *Decent* performance compared to Python.

## Non-goals:

- Additional features from Jinja that aren't used by the template(s) of any major LLM (no feature creep!)
    - Please don't submit PRs with such features, they will unfortunately be rejected.
- Full Jinja compliance (neither syntax-wise, nor filters / tests / globals)

## Usage:

See API in [minja/minja.hpp](./include/minja/chat-template.hpp) and [minja/chat-template.h](./include/minja/chat-template.hpp) (experimental).

For raw Jinja templating (see [examples/raw.cpp](./examples/raw.cpp)):

```c++
#include <minja.hpp>
#include <iostream>

using json = nlohmann::ordered_json;

int main() {
    auto tmpl = minja::Parser::parse("Hello, {{ location }}!", /* options= */ {});
    auto context = minja::Context::make(minja::Value(json {
        {"location", "World"},
    }));
    auto result = tmpl->render(context);
    std::cout << result << std::endl;
}
```

To apply a template to a JSON array of `messages` and `tools` in the HuggingFace standard (see [examples/chat-template.cpp](./examples/chat-template.cpp)):

```c++
#include <chat-template.hpp>
#include <iostream>

using json = nlohmann::ordered_json;

int main() {
    minja::chat_template tmpl(
        "{% for message in messages %}"
        "{{ '<|' + message['role'] + '|>\\n' + message['content'] + '<|end|>' + '\\n' }}"
        "{% endfor %}",
        /* bos_token= */ "<|start|>",
        /* eos_token= */ "<|end|>"
    );
    std::cout << tmpl.apply(
        json::parse(R"([
            {"role": "user", "content": "Hello"},
            {"role": "assistant", "content": "Hi there"}
        ])"),
        json::parse(R"([
            {"type": "function", "function": {"name": "google_search", "arguments": {"query": "2+2"}}}
        ])"),
        /* add_generation_prompt= */ true,
        /* extra_context= */ {}) << std::endl;
}
```

(Note that some template quirks are worked around by [minja/chat-template.hpp](./include/minja/chat-template.hpp) so that all templates can be used the same way)

## Supported features

Models have increasingly complex templates (e.g. [NousResearch/Hermes-3-Llama-3.1](./third_party/templates/NousResearch-Hermes-3-Llama-3.1-70B-tool_use.jinja), [meetkai/functionary-medium-v3.2](./third_party/templates/meetkai-functionary-medium-v3.2.jinja)), so a fair bit of Jinja's language constructs is required to execute their templates properly.

Minja supports:

- Full expression syntax
- Statements `{{% … %}}`, variable sections `{{ … }}`, and comments `{# … #}` with pre/post space elision `{%- … -%}` / `{{- … -}}` / `{#- … -#}`
- `if` / `elif` / `else` / `endif`
- `for` (`recursive`) (`if`) / `else` / `endfor` w/ `loop.*` (including `loop.cycle`) and destructuring
- `set` w/ namespaces & destructuring
- `macro` / `endmacro`
- Extensible filters collection: `count`, `dictsort`, `equalto`, `e` / `escape`, `items`, `join`, `joiner`, `namespace`, `raise_exception`, `range`, `reject`, `tojson`, `trim`

Main limitations (non-exhaustive list):

- Not supporting [most filters](https://jinja.palletsprojects.com/en/3.0.x/templates/#builtin-filters). Only the ones actually used in templates of major (or trendy) models are/will be implemented.
- No difference between `none` and `undefined`
- Single namespace with all filters / tests / functions / macros / variables
- No tuples (templates seem to rely on lists only)
- No `if` expressions w/o `else` (but `if` statements are fine)
- No `{% raw %}`, `{% block … %}`, `{% include … %}`, `{% extends … %},

## Roadmap / TODOs

- Setup github CI
- Fix known issues w/ CRLF on Windows
- Setup fuzzing w/ https://github.com/google/fuzztest
- Setup performance tests
- Integrate to llama.cpp: https://github.com/ggerganov/llama.cpp/pull/9639
- Simplify two-pass parsing
    - Pass tokens to IfNode and such
- Macro nested set scope = global?
- Get listed in https://jbmoelker.github.io/jinja-compat-tests/, https://en.cppreference.com/w/cpp/links/libs

## Developer corner

### Design overview

- `minja::Parser` does two-phased parsing:
  - its `tokenize()` method creates coarse template "tokens" (plain text section, or expression blocks or opening / closing blocks). Tokens may have nested expressions ASTs, parsed with `parseExpression()`
  - its `parseTemplate()` method iterates on tokens to build the final `TemplateNode` AST.
- `minja::Value` represents a Python-like value
  - It relies on `nlohmann/json` for primitive values, but does its own JSON dump to be exactly compatible w/ the Jinja / Python implementation of `dict` string representation
- `minja::chat_template` wraps a template and provides an interface similar to HuggingFace's chat template formatting. It also normalizes the message history to accommodate different expectations from some templates (e.g. `message.tool_calls.function.arguments` is typically expected to be a JSON string representation of the tool call arguments, but some templates expect the arguments object instead)
- [update_templates_and_goldens.py](./update_templates_and_goldens.py) fetches many templates, and runs them w/ the official Jinja2 library against a set of [tests/contexts](./tests/contexts) to create [tests/goldens](./tests/goldens) files. Then [test-chat-templates](./tests/test-chat-templates.cpp) ensures Minja produces exactly the same output as the goldens.

### Adding new Templates / Building

Prerequisites:

- cmake, flake8, editorconfig-checker

To add new templates, edit [update_templates_and_goldens.py](./update_templates_and_goldens.py) and run it (e.g. w/ [uv](https://github.com/astral-sh/uv)):

```bash
uv run update_templates_and_goldens.py
```

Then build & run the minja tests:

```bash
rm -fR build && \
    cmake -B build && \
    cmake --build build -j && \
    ctest --test-dir build/tests -j --output-on-failure
```
