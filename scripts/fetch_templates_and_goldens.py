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
    python tests/fetch_templates_and_goldens.py output_folder context_file1.json context_file2.json ... model_id1 model_id2 ...

  Example:
    pip install -r requirements.txt
    python tests/fetch_templates_and_goldens.py ./test_files tests/contexts/*.json microsoft/Phi-3-medium-4k-instruct Qwen/Qwen2-7B-Instruct
'''

import logging
import datetime
import os
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


def tojson(x, ensure_ascii=False, indent=None, separators=None, sort_keys=False):
    return json.dumps(x, ensure_ascii=ensure_ascii, indent=indent, separators=separators, sort_keys=sort_keys)


TEST_DATE = os.environ.get('TEST_DATE', '2024-07-26')


def strftime_now(format):
    now = datetime.datetime.strptime(TEST_DATE, "%Y-%m-%d")
    return now.strftime(format)


def join_cmake_path(parent, child):
    '''
        On Windows, CMake will interpret any backslashes as escapes so we return / for path separators
    '''
    return '/'.join(x.replace(r'\\', '/') for x in (parent, child))

def handle_chat_template(output_folder, model_id, variant, template_src, context_files):
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
    env.filters['safe'] = lambda x: x
    env.filters['tojson'] = tojson
    env.globals['raise_exception'] = raise_exception
    env.globals['strftime_now'] = strftime_now

    template_handles_tools = 'tools' in template_src
    template_hates_the_system = 'System role not supported' in template_src

    template = env.from_string(template_src)

    for context_file in context_files:
        context_name = os.path.basename(context_file).replace(".json", "")
        with open(context_file, 'r') as f:
            context = json.load(f)

        if not template_handles_tools and 'tools' in context:
            continue

        if template_hates_the_system and any(m['role'] == 'system' for m in context['messages']):
            continue

        output_file = join_cmake_path(output_folder, f'{base_name}-{context_name}.txt')

        render_context = json.loads(json.dumps(context))

        if 'tool_call.arguments | items' in template_src or 'tool_call.arguments | tojson' in template_src:
            for message in render_context['messages']:
                if 'tool_calls' in message:
                    for tool_call in message['tool_calls']:
                        if tool_call.get('type') == 'function':
                            arguments = tool_call['function']['arguments']
                            tool_call['function']['arguments'] = json.loads(arguments)

        try:
            output = template.render(**render_context)
        except Exception as e1:
            for message in context["messages"]:
                if message.get("content") is None:
                    message["content"] = ""

            try:
                output = template.render(**render_context)
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
