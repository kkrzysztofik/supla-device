#!/usr/bin/env bash
set -euo pipefail

cmake -B build-ingecon -S . \
  -DSUPLA_LINUX_EXTENSION_DIRS=../../../porting/linux/extensions/ingecon
cmake --build build-ingecon
