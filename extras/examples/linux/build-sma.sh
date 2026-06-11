#!/usr/bin/env bash
# Build supla-device-linux with the native SMA RS485 extension.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

cmake -B build -DSUPLA_LINUX_EXTENSION_DIRS=../../../porting/linux/extensions/sma
cmake --build build -j"$(nproc)"

echo ""
echo "Binary: ${SCRIPT_DIR}/build/supla-device-linux"
echo "Config: ${SCRIPT_DIR}/supla-device-wr33-008.yaml"
echo ""
echo "Run:"
echo "  ./build/supla-device-linux -c supla-device-wr33-008.yaml --verbose"
