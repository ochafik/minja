#!/usr/bin/env python3
# Copyright 2024 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.
#
# SPDX-License-Identifier: MIT
"""
Test that verifies there are no duplicate chat templates.

This test computes MD5 checksums for all .jinja files in tests/templates/
and fails if any duplicates are found. Duplicate templates waste storage,
build time, and test execution time.

Usage:
    python test_no_duplicate_templates.py [templates_directory]

Returns:
    0 if no duplicates found (success)
    1 if duplicates found (failure)
"""

import hashlib
import sys
from pathlib import Path
from collections import defaultdict


def compute_md5(file_path: Path) -> str:
    """Compute MD5 hash of a file."""
    md5 = hashlib.md5()
    with open(file_path, 'rb') as f:
        for chunk in iter(lambda: f.read(8192), b''):
            md5.update(chunk)
    return md5.hexdigest()


def find_duplicate_templates(templates_dir: Path) -> dict[str, list[Path]]:
    """
    Find duplicate templates by MD5 hash.

    Returns:
        Dictionary mapping checksums to list of file paths with that checksum.
        Only includes checksums that appear more than once.
    """
    checksums = defaultdict(list)

    template_files = list(templates_dir.glob('*.jinja'))

    if not template_files:
        print(f"Warning: No .jinja files found in {templates_dir}", file=sys.stderr)
        return {}

    for template_file in template_files:
        checksum = compute_md5(template_file)
        checksums[checksum].append(template_file)

    # Only return checksums with duplicates
    duplicates = {
        checksum: files
        for checksum, files in checksums.items()
        if len(files) > 1
    }

    return duplicates


def main():
    # Get templates directory from argument or use default
    if len(sys.argv) > 1:
        templates_dir = Path(sys.argv[1])
    else:
        # Default: tests/templates relative to this script
        script_dir = Path(__file__).parent
        templates_dir = script_dir / 'templates'

    if not templates_dir.exists():
        print(f"Error: Templates directory not found: {templates_dir}", file=sys.stderr)
        return 1

    if not templates_dir.is_dir():
        print(f"Error: Not a directory: {templates_dir}", file=sys.stderr)
        return 1

    # Find duplicates
    duplicates = find_duplicate_templates(templates_dir)

    if not duplicates:
        template_count = len(list(templates_dir.glob('*.jinja')))
        print(f"✓ No duplicate templates found ({template_count} unique templates)")
        return 0

    # Report duplicates
    print(f"✗ Found {len(duplicates)} duplicate template(s):", file=sys.stderr)
    print(file=sys.stderr)

    for checksum, files in sorted(duplicates.items()):
        print(f"Checksum {checksum}:", file=sys.stderr)
        for file_path in sorted(files):
            print(f"  - {file_path.name}", file=sys.stderr)
        print(file=sys.stderr)

    total_duplicates = sum(len(files) - 1 for files in duplicates.values())
    print(f"Total: {total_duplicates} duplicate file(s) should be removed", file=sys.stderr)

    return 1


if __name__ == '__main__':
    sys.exit(main())
