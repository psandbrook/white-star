#!/bin/bash
set -eu -o pipefail

BUILD_DIR="$(pwd)"
SCRIPT_DIR="$(dirname "$(realpath "$0")")"

cd "$SCRIPT_DIR"
while true; do
  inotifywait -r -e modify src || true
  sleep 0.02
  ninja -C "$BUILD_DIR" || true
done
