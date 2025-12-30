# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT
import sys
import json
from jinja2 import Environment
import jinja2.ext
from pathlib import Path

def tojson(value, indent=None, ensure_ascii=False, sort_keys=False, separators=None):
    return json.dumps(value, indent=indent, ensure_ascii=ensure_ascii, sort_keys=sort_keys, separators=separators)

input_file, output_file = sys.argv[1:3]
data = json.loads(Path(input_file).read_text())
# print(json.dumps(data, indent=2), file=sys.stderr)

env = Environment(**data['options'], extensions=[jinja2.ext.loopcontrols])
env.filters['tojson'] = tojson
tmpl = env.from_string(data['template'])
output = tmpl.render(data['bindings'])
Path(output_file).write_text(output)
