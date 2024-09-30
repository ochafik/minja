# minja.hpp - A minimalistic Jinja templating engine for LLM chat templates

_**This is not an official Google product**_

Minja is a minimalistic reimplementation of the [Jinja](https://github.com/pallets/jinja/) templating engine for C++ LLM projects (such as [llama.cpp](https://github.com/ggerganov/llama.cpp)).

It is **not general purpose**: it includes just what’s needed for actual chat templates (very limited set of filters, tests and language features). Users with different needs should look at third-party alternatives such as [Jinja2Cpp](https://github.com/jinja2cpp/Jinja2Cpp).

## Design goals:

- Support each and every major LLM found on HuggingFace
    - See [update_templates_and_goldens.py](./update_templates_and_goldens.py) for list of models currently supported
- Keep codebase small (currently 2.5k LoC) and easy to understand
- Easy to integrate to projects such as [llama.cpp](https://github.com/ggerganov/llama.cpp):
  - Header-only
  - C++11
  - Only depend on [nlohmann::json](https://github.com/nlohmann/json) (no Boost)
- *Decent* performance compared to Python.

## Non-goals:

- Additional features from Jinja that aren't used by the template(s) of any major LLM (no feature creep!)
    - Please don't submit PRs with such features, they will unfortunately be rejected.
- Full Jinja compliance (neither syntax-wise, nor filters / tests / globals)

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
- Integrate to llama.cpp: https://github.com/ggerganov/llama.cpp/pull/9639
- Simplify two-pass parsing
    - Pass tokens to IfNode and such
- Macro nested set scope = global?
- List in / link to https://jbmoelker.github.io/jinja-compat-tests/

## Develop

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
