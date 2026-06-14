#!/usr/bin/env bash
# Build supla-device-linux with both native SMA RS485 and Ingecon extensions.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
EXTENSIONS="../../../porting/linux/extensions/sma;../../../porting/linux/extensions/ingecon"

cmake -B "${BUILD_DIR}" -S "${SCRIPT_DIR}" \
  -DSUPLA_LINUX_EXTENSION_DIRS="${EXTENSIONS}"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo ""
echo "Binary: ${BUILD_DIR}/supla-device-linux"
echo "Configs:"
echo "  ${SCRIPT_DIR}/supla-device-wr33-008.yaml"
echo "  ${SCRIPT_DIR}/supla-device-ingecon.yaml"
echo ""
echo "Run SMA:"
echo "  ./build/supla-device-linux -c supla-device-wr33-008.yaml --verbose"
echo "Run Ingecon:"
echo "  ./build/supla-device-linux -c supla-device-ingecon.yaml --verbose"
