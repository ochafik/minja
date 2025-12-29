#!/bin/bash
# Script to build and run the tool-thinking template tests

set -e

echo "================================"
echo "Tool & Thinking Template Tests"
echo "================================"
echo

# Build the project
echo "Step 1: Configuring CMake..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

echo
echo "Step 2: Building test target..."
cmake --build build --target test-tool-thinking-template -j8

echo
echo "Step 3: Running tests..."
./build/tests/test-tool-thinking-template

echo
echo "================================"
echo "All tests completed!"
echo "================================"
