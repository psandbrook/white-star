#!/bin/bash
SCRIPT_DIR="$(realpath "$(dirname "$0")")"

cd "$SCRIPT_DIR"
find src -type f \( -name '*.cpp' -o -name '*.hpp' \) -exec clang-format -i -style=file {} +
