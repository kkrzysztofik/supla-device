#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
BUILD_DIR="${ROOT}/build-linux-byd"

cmake -S "${ROOT}/extras/examples/linux" -B "${BUILD_DIR}" \
  -DSUPLA_LINUX_EXTENSION_DIRS="${ROOT}/extras/porting/linux/extensions/byd"

cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "Built supla-device-linux with BYD extension in ${BUILD_DIR}"
