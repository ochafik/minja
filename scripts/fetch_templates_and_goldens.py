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

from dataclasses import dataclass
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

# data class
@dataclass
class TemplateCaps:
    supports_tools: bool = False
    supports_tool_calls: bool = False
    supports_tool_responses: bool = False
    supports_system_role: bool = False
    supports_parallel_tool_calls: bool = False
    supports_tool_call_id: bool = False
    requires_object_arguments: bool = False
    requires_typed_content: bool = False
    
    def to_json(self):
        return json.dumps(self.__dict__, indent=2)
    
def detect_caps(template_file, template):
    
    basic_extra_context = {
        "bos_token": "<|startoftext|>",
        "eos_token": "<|endoftext|>",
    }
    def try_raw_render(messages, *, tools=[], add_generation_prompt=False, extra_context={}, expect_strings=[]):
        try:
            return template.render(messages=messages, tools=tools, add_generation_prompt=add_generation_prompt, **basic_extra_context, **extra_context)
        except BaseException as e:
            # print(f"{template_file}: Error rendering template with messages {messages}: {e}", file=sys.stderr, flush=True)
            return ""
    
    caps = TemplateCaps()
    
    
    dummy_str_user_msg = {"role": "user", "content": "Hey" }
    dummy_typed_user_msg = {"role": "user", "content": [{"type": "text", "text": "Hey"}]}
    
    caps.requires_typed_content = \
        "Hey" not in try_raw_render([dummy_str_user_msg]) \
        and "Hey" in try_raw_render([dummy_typed_user_msg])
    dummy_user_msg = dummy_typed_user_msg if caps.requires_typed_content else dummy_str_user_msg
    
    needle = "<System Needle>"
    needle_system_msg = {"role": "system", "content": [{"type": "text", "text": needle}] if caps.requires_typed_content else needle}
    
    # caps_.supports_system_role = contains(try_raw_render({needle_system_msg, dummy_user_msg,}, {}, false), needle);
    caps.supports_system_role = needle in try_raw_render([needle_system_msg, dummy_user_msg])
    
    caps.supports_tools = "some_tool" in try_raw_render([dummy_user_msg], tools=[{
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
    }])
    
    def make_tool_calls_msg(tool_calls):
        return {
            "role": "assistant",
            "content": None,
            "tool_calls": tool_calls,
        }
    def make_tool_call(tool_name, arguments):
        return {
            "id": "call_1___",
            "type": "function",
            "function": {
                "arguments": arguments,
                "name": tool_name,
            }
        }
    
    dummy_args_obj = {"code": "print('Hello, World!')"}

    tool_call_renders_str_arguments = '{"code":' in try_raw_render([
        dummy_user_msg,
        make_tool_calls_msg([make_tool_call("ipython", json.dumps(dummy_args_obj))])
    ])
    tool_call_renders_obj_arguments = '{"code":' in try_raw_render([
        dummy_user_msg,
        make_tool_calls_msg([make_tool_call("ipython", dummy_args_obj)])
    ])

    caps.supports_tool_calls = tool_call_renders_str_arguments or tool_call_renders_obj_arguments
    caps.requires_object_arguments = not tool_call_renders_str_arguments and tool_call_renders_obj_arguments

    if caps.supports_tool_calls:
        dummy_args = dummy_args_obj if caps.requires_object_arguments else json.dumps(dummy_args_obj)
        tc1 = make_tool_call("test_tool1", dummy_args)
        tc2 = make_tool_call("test_tool2", dummy_args)
        out = try_raw_render([
            dummy_user_msg,
            make_tool_calls_msg([tc1, tc2]),
        ])
        caps.supports_parallel_tool_calls = "test_tool1" in out and "test_tool2" in out
        
        out = try_raw_render([
            dummy_user_msg,
            make_tool_calls_msg([tc1]),
            {
                "role": "tool",
                "name": "test_tool1",
                "content": "Some response!",
                "tool_call_id": "call_911_",
            }
        ])
        caps.supports_tool_responses = "Some response!" in out
        caps.supports_tool_call_id = "call_911_" in out
    
    return caps
    
def handle_chat_template(output_folder, model_id, variant, template_src, context_files):

    if '{% generation %}' in template_src:
        print('Removing {% generation %} blocks from template', file=sys.stderr)
        template_src = template_src.replace('{% generation %}', '').replace('{% endgeneration %}', '')

    model_name = model_id.replace("/", "-")
    base_name = f'{model_name}-{variant}' if variant else model_name
    template_file = join_cmake_path(output_folder, f'{base_name}.jinja')
    caps_file = join_cmake_path(output_folder, f'{base_name}.caps.json')

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

    caps = detect_caps(template_file, template)
    
    with open(caps_file, 'w') as f:
        f.write(caps.to_json())
    
    for context_file in context_files:
        context_name = os.path.basename(context_file).replace(".json", "")
        with open(context_file, 'r') as f:
            context = json.load(f)

        has_tools = 'tools' in context
        needs_tools_in_system = has_tools and not caps.supports_tools
        
        if not caps.supports_tool_calls and has_tools:
            print(f'Skipping {context_name} test as tools seem unsupported by template {template_file}', file=sys.stderr)
            continue
        
        if not caps.supports_system_role and (any(m['role'] == 'system' for m in context['messages']) or needs_tools_in_system):
            continue

        output_file = join_cmake_path(output_folder, f'{base_name}-{context_name}.txt')

        if needs_tools_in_system:
            add_system(context['messages'], f"Available tools: {json.dumps(context['tools'], indent=2)}")

        for message in context['messages']:
            if 'tool_calls' in message:
                for tool_call in message['tool_calls']:
                    if caps.requires_object_arguments:
                        if tool_call.get('type') == 'function':
                            arguments = tool_call['function']['arguments']
                            try:
                                arguments = json.loads(arguments)
                            except:
                                pass
                            tool_call['function']['arguments'] = arguments
                if not caps.supports_tool_calls:
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
            if message.get('role') == 'tool' and not caps.supports_tool_responses:
                message['role'] = 'user'
                message['content'] = json.dumps({
                    "tool_response": {
                        "tool": message['name'],
                        "content": message['content'],
                        "tool_call_id": message.get('tool_call_id'),
                    }
                }, indent=2)
                del message['name']

        if caps.requires_typed_content:
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
        print(f"{template_file} {caps_file} {context_file} {output_file}")


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
            logger.error(f"Error processing model {model_id}: {e}", e)
            handle_chat_template(output_folder, model_id, None, str(e), [])


if __name__ == '__main__':
    main()
