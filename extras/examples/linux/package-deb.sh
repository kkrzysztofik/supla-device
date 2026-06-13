#!/bin/bash
#
# Copyright (C) 2026 Krzysztof Krzysztofik
#
# Build a simple Debian package for supla-device-linux.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PACKAGE_NAME="${PACKAGE_NAME:-supla-device-linux}"
BINARY_PATH="${BINARY_PATH:-${SCRIPT_DIR}/build/supla-device-linux}"
CONFIG_PATH="${CONFIG_PATH:-${SCRIPT_DIR}/supla-device.yaml}"
UNIT_PATH="${UNIT_PATH:-${SCRIPT_DIR}/supla-device-linux.service}"
OUTPUT_DIR="${OUTPUT_DIR:-${SCRIPT_DIR}/build/deb}"
MAINTAINER="${MAINTAINER:-Krzysztof Krzysztofik <root@localhost>}"
PACKAGE_VERSION="${PACKAGE_VERSION:-}"
ARCHITECTURE="${ARCHITECTURE:-}"

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --binary PATH       supla-device-linux binary (default: build/supla-device-linux)
  --config PATH       source config YAML (default: supla-device.yaml)
  --unit PATH         systemd unit template (default: supla-device-linux.service)
  --output-dir PATH   output directory (default: build/deb)
  --version VERSION   Debian package version (default: git-derived)
  --arch ARCH         Debian architecture (default: dpkg --print-architecture)
  -h, --help          show this help

Environment overrides:
  PACKAGE_NAME, BINARY_PATH, CONFIG_PATH, UNIT_PATH, OUTPUT_DIR,
  MAINTAINER, PACKAGE_VERSION, ARCHITECTURE
EOF
}

need_arg() {
  if [ "$#" -lt 2 ] || [ -z "$2" ]; then
    echo "Missing value for option: $1" >&2
    exit 2
  fi
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --binary)
      need_arg "$@"
      BINARY_PATH="$2"
      shift 2
      ;;
    --config)
      need_arg "$@"
      CONFIG_PATH="$2"
      shift 2
      ;;
    --unit)
      need_arg "$@"
      UNIT_PATH="$2"
      shift 2
      ;;
    --output-dir)
      need_arg "$@"
      OUTPUT_DIR="$2"
      shift 2
      ;;
    --version)
      need_arg "$@"
      PACKAGE_VERSION="$2"
      shift 2
      ;;
    --arch)
      need_arg "$@"
      ARCHITECTURE="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required command: $1" >&2
    exit 1
  fi
}

deb_version_from_git() {
  local desc

  if desc="$(git -C "$SCRIPT_DIR" describe --tags --dirty --always 2>/dev/null)"; then
    desc="${desc#v}"
  else
    desc="0"
  fi

  case "$desc" in
    [0-9]*)
      ;;
    *)
      desc="0+git${desc}"
      ;;
  esac

  printf '%s' "$desc" | sed 's/[^A-Za-z0-9.+:~_-]/-/g; s/_/-/g'
}

normalize_path() {
  local path="$1"
  local dir

  dir="$(cd "$(dirname "$path")" && pwd)"
  printf '%s/%s' "$dir" "$(basename "$path")"
}

require_cmd dpkg
require_cmd dpkg-deb
require_cmd dpkg-shlibdeps
require_cmd install
require_cmd sed

BINARY_PATH="$(normalize_path "$BINARY_PATH")"
CONFIG_PATH="$(normalize_path "$CONFIG_PATH")"
UNIT_PATH="$(normalize_path "$UNIT_PATH")"
OUTPUT_DIR="$(mkdir -p "$OUTPUT_DIR" && cd "$OUTPUT_DIR" && pwd)"

if [ ! -x "$BINARY_PATH" ]; then
  echo "Missing executable binary: $BINARY_PATH" >&2
  echo "Build it first, for example:" >&2
  echo "  cmake -S \"$SCRIPT_DIR\" -B \"$SCRIPT_DIR/build\"" >&2
  echo "  cmake --build \"$SCRIPT_DIR/build\"" >&2
  exit 1
fi

if [ ! -f "$CONFIG_PATH" ]; then
  echo "Missing config template: $CONFIG_PATH" >&2
  exit 1
fi

if [ ! -f "$UNIT_PATH" ]; then
  echo "Missing systemd unit template: $UNIT_PATH" >&2
  exit 1
fi

if [ -z "$PACKAGE_VERSION" ]; then
  PACKAGE_VERSION="$(deb_version_from_git)"
fi

if [ -z "$ARCHITECTURE" ]; then
  ARCHITECTURE="$(dpkg --print-architecture)"
fi

WORK_DIR="$(mktemp -d "${OUTPUT_DIR}/${PACKAGE_NAME}.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

PKG_ROOT="${WORK_DIR}/pkg"
DEBIAN_DIR="${PKG_ROOT}/DEBIAN"

install -d "$DEBIAN_DIR"
install -d "${PKG_ROOT}/usr/bin"
install -d "${PKG_ROOT}/etc"
install -d "${PKG_ROOT}/lib/systemd/system"
install -d "${PKG_ROOT}/var/lib/supla-device"

install -m 0755 "$BINARY_PATH" "${PKG_ROOT}/usr/bin/supla-device-linux"

if grep -q '^state_files_path:' "$CONFIG_PATH"; then
  sed 's#^state_files_path:.*#state_files_path: "/var/lib/supla-device"#' \
    "$CONFIG_PATH" >"${PKG_ROOT}/etc/supla-device.yaml"
else
  cp "$CONFIG_PATH" "${PKG_ROOT}/etc/supla-device.yaml"
  printf '\nstate_files_path: "/var/lib/supla-device"\n' \
    >>"${PKG_ROOT}/etc/supla-device.yaml"
fi

sed 's#/usr/local/bin/supla-device-linux#/usr/bin/supla-device-linux#g' \
  "$UNIT_PATH" >"${PKG_ROOT}/lib/systemd/system/supla-device.service"

chmod 0644 "${PKG_ROOT}/etc/supla-device.yaml"
chmod 0644 "${PKG_ROOT}/lib/systemd/system/supla-device.service"
chmod 0750 "${PKG_ROOT}/var/lib/supla-device"

cat >"${DEBIAN_DIR}/conffiles" <<EOF
/etc/supla-device.yaml
EOF

cat >"${DEBIAN_DIR}/postinst" <<'EOF'
#!/bin/sh
set -e

if ! getent group supla-device >/dev/null; then
  addgroup --system supla-device >/dev/null
fi

if ! getent passwd supla-device >/dev/null; then
  adduser --system \
    --ingroup supla-device \
    --home /var/lib/supla-device \
    --no-create-home \
    --shell /usr/sbin/nologin \
    supla-device >/dev/null
fi

if getent group dialout >/dev/null; then
  usermod -a -G dialout supla-device || true
fi

install -d -o supla-device -g supla-device -m 0750 /var/lib/supla-device

if command -v systemctl >/dev/null 2>&1; then
  systemctl daemon-reload || true
fi

exit 0
EOF

cat >"${DEBIAN_DIR}/prerm" <<'EOF'
#!/bin/sh
set -e

if [ "$1" = "remove" ] || [ "$1" = "deconfigure" ]; then
  if command -v systemctl >/dev/null 2>&1; then
    systemctl stop supla-device.service || true
  fi
fi

exit 0
EOF

cat >"${DEBIAN_DIR}/postrm" <<'EOF'
#!/bin/sh
set -e

if command -v systemctl >/dev/null 2>&1; then
  systemctl daemon-reload || true
fi

exit 0
EOF

chmod 0755 "${DEBIAN_DIR}/postinst" "${DEBIAN_DIR}/prerm" "${DEBIAN_DIR}/postrm"

mkdir -p "${WORK_DIR}/debian"
cat >"${WORK_DIR}/debian/control" <<EOF
Source: ${PACKAGE_NAME}
Section: utils
Priority: optional
Maintainer: ${MAINTAINER}
Standards-Version: 4.7.0

Package: ${PACKAGE_NAME}
Architecture: any
Depends: \${shlibs:Depends}
Description: SUPLA Device Linux runtime
 SUPLA-compatible device implementation for Linux.
EOF

SHLIBS_DEPENDS="$(
  cd "$WORK_DIR"
  dpkg-shlibdeps -O "pkg/usr/bin/supla-device-linux"
)"
SHLIBS_DEPENDS="${SHLIBS_DEPENDS#shlibs:Depends=}"
DEPENDS="${SHLIBS_DEPENDS}, ca-certificates, adduser, systemd"
INSTALLED_SIZE="$(du -sk "$PKG_ROOT" | cut -f1)"

cat >"${DEBIAN_DIR}/control" <<EOF
Package: ${PACKAGE_NAME}
Version: ${PACKAGE_VERSION}
Section: utils
Priority: optional
Architecture: ${ARCHITECTURE}
Maintainer: ${MAINTAINER}
Depends: ${DEPENDS}
Installed-Size: ${INSTALLED_SIZE}
Description: SUPLA Device Linux runtime
 SUPLA-compatible device implementation for Linux.
EOF

DEB_PATH="${OUTPUT_DIR}/${PACKAGE_NAME}_${PACKAGE_VERSION}_${ARCHITECTURE}.deb"
dpkg-deb --root-owner-group --build "$PKG_ROOT" "$DEB_PATH"

echo "$DEB_PATH"
