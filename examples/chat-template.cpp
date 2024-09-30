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
