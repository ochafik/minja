# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT
'''
  Fetches the Jinja2 templates of specified models and generates prompt goldens for given chat contexts.
  Outputs lines of arguments for a C++ test binary.
  All files are written to the specified output folder.

  Usage:
    python scripts/fetch_templates_and_goldens.py output_folder context_file1.json context_file2.json ... model_id1 model_id2 ...

  Example:
    pip install -r requirements.txt
    python scripts/fetch_templates_and_goldens.py ./test_files tests/contexts/*.json mistralai/Mistral-Large-Instruct-2407 meetkai/functionary-medium-v3.1.jinja microsoft/Phi-3-medium-4k-instruct Qwen/Qwen2-7B-Instruct
'''

import logging
import datetime
import os
import sys
from huggingface_hub import hf_hub_download
import json
import jinja2
import jinja2.ext
import re
import argparse
import shutil

logging.basicConfig(level=logging.INFO, format='%(message)s')
logger = logging.getLogger(__name__)


def raise_exception(message: str):
    raise ValueError(message)


def tojson(eval_ctx, value, indent=None):
    return json.dumps(value, indent=indent)

TEST_DATE = os.environ.get('TEST_DATE', '2024-07-26')


def strftime_now(format):
    now = datetime.datetime.strptime(TEST_DATE, "%Y-%m-%d")
    return now.strftime(format)


def join_cmake_path(parent, child):
    '''
        On Windows, CMake will interpret any backslashes as escapes so we return / for path separators
    '''
    return '/'.join(x.replace(r'\\', '/') for x in (parent, child))


def add_system(messages, system_prompt):
    if len(messages) > 0 and messages[0]["role"] == "system":
        existing_system = messages[0]["content"]
        messages[0] = {
            "role": "system",
            "content": existing_system + "\n" + system_prompt,
        }
    else:
        messages.insert(0, {
            "role": "system",
            "content": system_prompt,
        })

    
def handle_chat_template(output_folder, model_id, variant, template_src, context_files):

    if '{% generation %}' in template_src:
        print('Removing {% generation %} blocks from template', file=sys.stderr)
        template_src = template_src.replace('{% generation %}', '').replace('{% endgeneration %}', '')

    model_name = model_id.replace("/", "-")
    base_name = f'{model_name}-{variant}' if variant else model_name
    template_file = join_cmake_path(output_folder, f'{base_name}.jinja')

    with open(template_file, 'w') as f:
        f.write(template_src)

    if not context_files:
        print(f"{template_file} n/a {template_file}")
        return

    env = jinja2.Environment(
        trim_blocks=True,
        lstrip_blocks=True,
        extensions=[jinja2.ext.loopcontrols]
    )
    template = env.from_string(template_src)
    
    env.filters['safe'] = lambda x: x
    env.filters['tojson'] = tojson
    env.globals['raise_exception'] = raise_exception
    env.globals['strftime_now'] = strftime_now

    def renders(messages, *, tools=[], add_generation_prompt=False, extra_context={}, expect_strings=[]):
        try:
            prompt = template.render(messages=messages, tools=tools, add_generation_prompt=add_generation_prompt, **extra_context)
            for str in expect_strings:
                if str not in prompt:
                    # print(f"Expected string not found: {str}\nin prompt:\n{prompt}", file=sys.stderr, flush=True)
                    return False
            return True
        except BaseException as e:
            print(f"{template_file}: Error rendering template with messages {messages}: {e}", file=sys.stderr, flush=True)
            return False
    
    basic_extra_context = {
        "bos_token": "<|startoftext|>",
        "eos_token": "<|endoftext|>",
    }
    
    # const json dummy_str_user_msg = {{"role", "user"}, {"content", "Hey"}};
    # const json dummy_typed_user_msg = {{"role", "user"}, {"content", json::array({{{"type", "text"}, {"text", "Hey"}}})}};

    # requires_typed_content_ =
    #     !contains(try_raw_render({{dummy_str_user_msg}}, {}, false), "Hey")
    #     && contains(try_raw_render({{dummy_typed_user_msg}}, {}, false), "Hey");

    # const auto dummy_user_msg = requires_typed_content_
    #     ? dummy_typed_user_msg
    #     : dummy_str_user_msg;
    dummy_str_user_msg = {"role": "user", "content": "Hey" }
    dummy_typed_user_msg = {"role": "user", "content": [{"type": "text", "text": "Hey"}]}
    
    requires_typed_content = \
        not renders([dummy_str_user_msg], extra_context=basic_extra_context, expect_strings=["Hey"]) \
        and renders([dummy_typed_user_msg], extra_context=basic_extra_context, expect_strings=["Hey"])
    dummy_user_msg = dummy_typed_user_msg if requires_typed_content else dummy_str_user_msg
    
    needle = "<System Needle>"
    needle_system_msg = {"role": "system", "content": [{"type": "text", "text": needle}] if requires_typed_content else needle}
    
    supports_code_interpreter = 'code_interpreter' in template_src
    supports_parallel_tool_calls = 'tool_call_id' in template_src
    supports_tool_calls = 'tool_calls' in template_src
    supports_system_role = renders([needle_system_msg, dummy_user_msg], extra_context=basic_extra_context, expect_strings=[needle])
    supports_tools = renders([dummy_user_msg], tools=[{
        "type": "function",
        "function": {
            "name": "some_tool",
            "description": "Some tool",
            "parameters": {
                "type": "object",
                "properties": {
                    "arg": "string",
                },
                "required": ["arg"],
            },
        },
    }], extra_context=basic_extra_context, expect_strings=["some_tool"])
    
    requires_object_arguments = \
        renders([
            dummy_user_msg,
            {"role": "assistant", "content": "", "tool_calls": [{
            "id": "call_1___",
            "type": "function",
            "function": {
                "arguments": {"code": "print('Hello, World!')"},
                "name": "ipython"
            }
            }]}
        ], extra_context=basic_extra_context, expect_strings=[r'{"code": "print']) \
        and not renders([
            dummy_user_msg,
            {"role": "assistant", "content": "", "tool_calls": [{
            "id": "call_1___",
            "type": "function",
            "function": {
                "arguments": "{\"code\": \"print('Hello, World!')\"}",
                "name": "ipython"
            }
            }]}
        ], extra_context=basic_extra_context, expect_strings=[r'{"code": "print'])
    
    for context_file in context_files:
        context_name = os.path.basename(context_file).replace(".json", "")
        with open(context_file, 'r') as f:
            context = json.load(f)

        has_tools = 'tools' in context
        needs_tools_in_system = has_tools and not supports_tools
        
        if not supports_tool_calls and has_tools:
            print(f'Skipping {context_name} test as tools seem unsupported by template {template_file}', file=sys.stderr)
            continue
        
        if not supports_system_role and (any(m['role'] == 'system' for m in context['messages']) or needs_tools_in_system):
            continue

        output_file = join_cmake_path(output_folder, f'{base_name}-{context_name}.txt')

        if needs_tools_in_system:
            add_system(context['messages'], f"Available tools: {json.dumps(context['tools'], indent=2)}")

        for message in context['messages']:
            if 'tool_calls' in message:
                for tool_call in message['tool_calls']:
                    if requires_object_arguments:
                        if tool_call.get('type') == 'function':
                            arguments = tool_call['function']['arguments']
                            try:
                                arguments = json.loads(arguments)
                            except:
                                pass
                            tool_call['function']['arguments'] = arguments
                if not supports_tool_calls:
                    message['content'] = json.dumps({
                        "tool_calls": [
                            {
                                "name": tc['function']['name'],
                                "arguments": json.loads(tc['function']['arguments']),
                                "id": tc.get('id'),
                            }
                            for tc in message['tool_calls']
                        ],
                        "content": None if message.get('content', '') == '' else message['content'],
                    }, indent=2)
                    del message['tool_calls']
            if message.get('role') == 'tool' and not supports_tools:
                message['role'] = 'user'
                message['content'] = json.dumps({
                    "tool_response": {
                        "tool": message['name'],
                        "content": message['content'],
                        "tool_call_id": message.get('tool_call_id'),
                    }
                }, indent=2)
                del message['name']

        if requires_typed_content:
            for message in context['messages']:
                if 'content' in message and isinstance(message['content'], str):
                    message['content'] = [{"type": "text", "text": message['content']}]

        # print(json.dumps(context, indent=2), file=sys.stderr)
        try:
            output = template.render(**context)
        except Exception as e1:
            for message in context['messages']:
                if message.get("content") is None:
                    message["content"] = ""

            try:
                output = template.render(**context)
            except Exception as e2:
                logger.info(f"  ERROR: {e2} (after first error: {e1})")
                output = f"ERROR: {e2}"

        with open(output_file, 'w') as f:
            f.write(output)

        # Output the line of arguments for the C++ test binary
        print(f"{template_file} {context_file} {output_file}")


def main():
    parser = argparse.ArgumentParser(description="Generate chat templates and output test arguments.")
    parser.add_argument("output_folder", help="Folder to store all output files")
    parser.add_argument("json_context_files_or_model_ids", nargs="+", help="List of context JSON files or HuggingFace model IDs")
    args = parser.parse_args()

    context_files = []
    model_ids = []
    for file in args.json_context_files_or_model_ids:
        if file.endswith('.json'):
            context_files.append(file)
        else:
            model_ids.append(file)

    output_folder = args.output_folder
    if not os.path.isdir(output_folder):
        os.makedirs(output_folder)

    # Copy context files to the output folder
    for context_file in context_files:
        shutil.copy(context_file, output_folder)

    for model_id in model_ids:
        try:
            with open(hf_hub_download(repo_id=model_id, filename="tokenizer_config.json")) as f:
                config_str = f.read()

            try:
                config = json.loads(config_str)
            except json.JSONDecodeError:
                config = json.loads(re.sub(r'\}([\n\s]*\}[\n\s]*\],[\n\s]*"clean_up_tokenization_spaces")', r'\1', config_str))

            assert 'chat_template' in config, 'No "chat_template" entry in tokenizer_config.json!'
            chat_template = config['chat_template']
            if isinstance(chat_template, str):
                handle_chat_template(output_folder, model_id, None, chat_template, context_files)
            else:
                for ct in chat_template:
                    handle_chat_template(output_folder, model_id, ct['name'], ct['template'], context_files)
        except Exception as e:
            logger.error(f"Error processing model {model_id}: {e}")
            handle_chat_template(output_folder, model_id, None, str(e), [])


if __name__ == '__main__':
    main()
