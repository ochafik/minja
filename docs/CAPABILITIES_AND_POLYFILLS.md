# Chat Template Capabilities and Polyfills

Minja automatically detects chat template capabilities and can apply polyfills to normalize differences between templates. This enables applications to use a single canonical message format while supporting a wide variety of model templates.

## Table of Contents

- [Capabilities Detection](#capabilities-detection)
- [Reasoning Formats](#reasoning-formats)
- [Automatic Polyfills](#automatic-polyfills)
- [Usage](#usage)
- [Examples](#examples)

## Capabilities Detection

When a `chat_template` is constructed, minja probes the template with test messages to detect its capabilities:

### Core Capabilities

| Capability | Description |
|------------|-------------|
| `supports_system_role` | Template renders system messages correctly |
| `supports_tools` | Template has native tool/function definition support |
| `supports_tool_calls` | Template renders assistant tool calls |
| `supports_tool_responses` | Template handles tool response messages |
| `supports_tool_call_id` | Template uses tool call IDs for correlation |
| `supports_parallel_tool_calls` | Template can handle multiple tool calls per message |

### Content Format Capabilities

| Capability | Description |
|------------|-------------|
| `requires_object_arguments` | Tool call arguments must be objects, not JSON strings |
| `requires_non_null_content` | Content field cannot be null (must be empty string) |
| `requires_typed_content_blocks` | Content must be `[{type: "text", text: ...}]` format |

### Reasoning Capabilities

| Capability | Description |
|------------|-------------|
| `supports_reasoning` | Template supports some form of reasoning/thinking |
| `reasoning_format` | Which `ReasoningFormat` the template uses |
| `reasoning_requires_tools` | Reasoning only works with tool_calls present |
| `supports_clear_thinking` | Template respects `clear_thinking` flag for visibility control |
| `supports_reasoning_without_content` | Can emit reasoning with empty content |
| `supports_reasoning_with_content` | Can emit both reasoning and content together |
| `respects_enable_reasoning` | Template honors `enable_thinking=false` |

## Reasoning Formats

Different models represent chain-of-thought reasoning in different ways. Minja detects and supports six formats:

### Field-Based Formats

| Format | Field | Example Models |
|--------|-------|----------------|
| `REASONING_CONTENT_FIELD` | `message.reasoning_content` | Qwen3, GLM-4.6/4.7 |
| `THOUGHT_FIELD` | `message.thought` | MiniCPM3 |
| `THINKING_FIELD` | `message.thinking` | GPT-OSS-120B |
| `TOOL_PLAN_FIELD` | `message.tool_plan` | Command-R7B (requires tool_calls) |

### Content Block Formats

| Format | Block Type | Example Models |
|--------|------------|----------------|
| `THINKING_CONTENT_BLOCK` | `{type: "thinking", thinking: ...}` | Ministral, DeepSeek-R1 |
| `THOUGHTS_CONTENT_BLOCK` | `{type: "thoughts", text: ...}` | Apertus, Kimi K2 |

### Detection Priority

Formats are detected in priority order:
1. `REASONING_CONTENT_FIELD` (canonical format)
2. `THOUGHT_FIELD`
3. `THINKING_FIELD`
4. `TOOL_PLAN_FIELD`
5. `THINKING_CONTENT_BLOCK`
6. `THOUGHTS_CONTENT_BLOCK`

## Automatic Polyfills

When `apply_polyfills` is enabled (default), minja automatically transforms messages to match what each template expects.

### Reasoning Polyfill

The canonical input format uses `reasoning_content`:

```json
{
  "role": "assistant",
  "reasoning_content": "Let me think about this...",
  "content": "The answer is 42."
}
```

This is automatically transformed based on the template's `reasoning_format`:

**For THOUGHT_FIELD (MiniCPM3):**
```json
{
  "role": "assistant",
  "thought": "Let me think about this...",
  "content": "The answer is 42."
}
```

**For THINKING_CONTENT_BLOCK (Ministral):**
```json
{
  "role": "assistant",
  "content": [
    {"type": "thinking", "thinking": "Let me think about this..."},
    {"type": "text", "text": "The answer is 42."}
  ]
}
```

**For THOUGHTS_CONTENT_BLOCK (Kimi K2):**
```json
{
  "role": "assistant",
  "content": [
    {"type": "thoughts", "text": "Let me think about this..."},
    {"type": "text", "text": "The answer is 42."}
  ]
}
```

### Other Polyfills

| Polyfill | What it does |
|----------|--------------|
| `polyfill_system_role` | Merges system messages into first user message |
| `polyfill_tools` | Adds tool definitions to system prompt |
| `polyfill_tool_calls` | Converts tool calls to text format |
| `polyfill_tool_responses` | Converts tool role to user with tool_response object |
| `polyfill_object_arguments` | Parses JSON string arguments to objects |
| `polyfill_typed_content` | Converts string content to `[{type: "text", text: ...}]` |

### Polyfill Options

All polyfills can be individually controlled via `chat_template_options`:

```cpp
chat_template_options opts;
opts.apply_polyfills = true;           // Master switch (default: true)
opts.polyfill_reasoning = true;        // Reasoning format conversion
opts.polyfill_typed_content = true;    // String → content blocks
opts.polyfill_system_role = true;      // System role emulation
opts.polyfill_tools = true;            // Tool definition injection
opts.polyfill_tool_calls = true;       // Tool call formatting
opts.polyfill_tool_responses = true;   // Tool response formatting
opts.polyfill_object_arguments = true; // Argument parsing
```

## Usage

### C++ API

```cpp
#include <minja/chat-template.hpp>

// Load template from model's tokenizer_config.json
std::string template_source = /* chat_template field */;
std::string bos_token = "<s>";
std::string eos_token = "</s>";

minja::chat_template tmpl(template_source, bos_token, eos_token);

// Check capabilities
auto caps = tmpl.original_caps();
if (caps.supports_reasoning) {
    std::cout << "Template supports reasoning format: "
              << static_cast<int>(caps.reasoning_format) << std::endl;
}

// Prepare messages with canonical format
nlohmann::json messages = {
    {{"role", "user"}, {"content", "What is 2+2?"}},
    {{"role", "assistant"},
     {"reasoning_content", "Let me calculate: 2+2=4"},
     {"content", "The answer is 4."}}
};

// Apply template (polyfills applied automatically)
minja::chat_template_inputs inputs;
inputs.messages = messages;
inputs.add_generation_prompt = true;

std::string prompt = tmpl.apply(inputs);
```

### Checking Specific Capabilities

```cpp
auto caps = tmpl.original_caps();

// Tool support
if (caps.supports_tool_calls && !caps.requires_object_arguments) {
    // Can use stringified JSON arguments
}

// Reasoning support
if (caps.supports_reasoning) {
    switch (caps.reasoning_format) {
        case minja::ReasoningFormat::REASONING_CONTENT_FIELD:
            // Native support, no polyfill needed
            break;
        case minja::ReasoningFormat::THINKING_CONTENT_BLOCK:
            // Will be polyfilled to content blocks
            break;
        // ... handle other formats
    }
}

// Content format
if (caps.requires_typed_content_blocks) {
    // Template expects [{type: "text", text: ...}] format
    // Polyfill will convert automatically if enabled
}
```

### Disabling Polyfills

```cpp
minja::chat_template_options opts;
opts.apply_polyfills = false;  // Disable all polyfills

// Or disable specific polyfills
opts.polyfill_reasoning = false;
opts.polyfill_typed_content = false;

std::string prompt = tmpl.apply(inputs, opts);
```

## Examples

### Example 1: Qwen3 (Native reasoning_content)

```cpp
// Qwen3 template natively supports reasoning_content
auto caps = tmpl.original_caps();
// caps.reasoning_format == ReasoningFormat::REASONING_CONTENT_FIELD
// caps.supports_reasoning == true

// Input (canonical format):
json msg = {
    {"role", "assistant"},
    {"reasoning_content", "Thinking..."},
    {"content", "Answer"}
};
// Output: No transformation needed, template handles reasoning_content directly
```

### Example 2: MiniCPM3 (thought field)

```cpp
// MiniCPM3 uses "thought" field
auto caps = tmpl.original_caps();
// caps.reasoning_format == ReasoningFormat::THOUGHT_FIELD

// Input (canonical format):
json msg = {
    {"role", "assistant"},
    {"reasoning_content", "Thinking..."},
    {"content", "Answer"}
};
// After polyfill:
// {"role": "assistant", "thought": "Thinking...", "content": "Answer"}
```

### Example 3: Kimi K2 (thoughts content block)

```cpp
// Kimi K2 uses content blocks with type="thoughts"
auto caps = tmpl.original_caps();
// caps.reasoning_format == ReasoningFormat::THOUGHTS_CONTENT_BLOCK
// caps.requires_typed_content_blocks == true

// Input (canonical format):
json msg = {
    {"role", "assistant"},
    {"reasoning_content", "Thinking..."},
    {"content", "Answer"}
};
// After polyfill:
// {
//   "role": "assistant",
//   "content": [
//     {"type": "thoughts", "text": "Thinking..."},
//     {"type": "text", "text": "Answer"}
//   ]
// }
```

### Example 4: Command-R7B (tool_plan with tool calls)

```cpp
// Command-R7B uses tool_plan field, but only with tool_calls
auto caps = tmpl.original_caps();
// caps.reasoning_format == ReasoningFormat::TOOL_PLAN_FIELD
// caps.reasoning_requires_tools == true

// Input with tool_calls:
json msg = {
    {"role", "assistant"},
    {"reasoning_content", "I need to search for this..."},
    {"content", nullptr},
    {"tool_calls", json::array({...})}
};
// After polyfill:
// {
//   "role": "assistant",
//   "tool_plan": "I need to search for this...",
//   "content": null,
//   "tool_calls": [...]
// }
```

## Integration with llama.cpp

When using minja in llama.cpp, the polyfill system enables:

1. **Unified Input Format**: Always use `reasoning_content` regardless of model
2. **Automatic Conversion**: Polyfills transform to each model's native format
3. **Simplified Parsing**: Output parsers can focus on canonical format
4. **Capability Queries**: Check what features a model's template supports

See the [llama.cpp integration branch](https://github.com/ochafik/llama.cpp/tree/sync-minja-reasoning) for implementation details.
