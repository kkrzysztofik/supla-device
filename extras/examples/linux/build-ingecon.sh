#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

cmake -B "$SCRIPT_DIR/build-ingecon" -S "$SCRIPT_DIR" \
  -DSUPLA_LINUX_EXTENSION_DIRS=../../../porting/linux/extensions/ingecon
cmake --build "$SCRIPT_DIR/build-ingecon"
