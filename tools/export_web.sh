#!/usr/bin/env bash
# Unix launcher for the shared Web exporter.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMSDK_DIR="${EMSDK_DIR:-/media/projectos/projects/emsdk}"
if [[ -f "$EMSDK_DIR/emsdk_env.sh" ]]; then
    source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
fi
exec python3 "$ROOT/tools/export_web.py" "$@"
