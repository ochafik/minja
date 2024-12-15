# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT
import sys
import json
import jinja2
import jinja2.ext

data = json.loads(sys.stdin.read())
# print(json.dumps(data, indent=2), file=sys.stderr)

print(jinja2.Environment(
    trim_blocks=data.get('trim_blocks', False),
    lstrip_blocks=data.get('lstrip_blocks', False),
    keep_trailing_newline=data.get('keep_trailing_newline', False),
    undefined=jinja2.DebugUndefined,
    extensions=[jinja2.ext.loopcontrols]
).from_string(data['template']).render(data['bindings'] or {}), end='')
