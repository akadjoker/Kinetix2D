#!/usr/bin/env bash
# Export a Kinetix project as a browser bundle. The engine template is cached;
# only project files and Zen bytecode are rebuilt on ordinary exports.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMSDK_DIR="${EMSDK_DIR:-/media/projectos/projects/emsdk}"
PROJECT="${1:-}"
RUN=0
PORT=8080
REBUILD=0
SCENE=""
shift || true
while [[ $# -gt 0 ]]; do
    case "$1" in
        --run) RUN=1 ;;
        --rebuild) REBUILD=1 ;;
        --port) PORT="$2"; shift ;;
        --scene) SCENE="$2"; shift ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done

if [[ -z "$PROJECT" || ! -f "$PROJECT/project.k2dproj" ]]; then
    echo "Usage: tools/export_web.sh <project-directory> [--scene path] [--run] [--rebuild] [--port N]" >&2
    exit 1
fi
if [[ ! -f "$EMSDK_DIR/emsdk_env.sh" ]]; then
    echo "Emscripten SDK not found: $EMSDK_DIR (set EMSDK_DIR)" >&2
    exit 1
fi

PROJECT="$(cd "$PROJECT" && pwd)"
GAME="$(basename "$PROJECT")"
BUILD="$ROOT/build-web"
TEMPLATE="$ROOT/bin/web/_template"
OUT="$PROJECT/.k2d/web"
STAGE="$(mktemp -d "${TMPDIR:-/tmp}/k2d-web-stage.XXXXXX")"
trap 'rm -rf "$STAGE"' EXIT

source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
if [[ "$REBUILD" == 1 || ! -f "$TEMPLATE/k2d_runner.js" || ! -f "$TEMPLATE/k2d_runner.wasm" ]]; then
    emcmake cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DK2D_RUNTIME_OUTPUT_DIRECTORY="$TEMPLATE"
    cmake --build "$BUILD" --target k2d_runner -j"$(nproc)"
fi

mkdir -p "$OUT" "$STAGE/.k2d/web"
rm -f "$OUT/k2d_runner.js" "$OUT/k2d_runner.wasm" "$OUT/assets.js" "$OUT/assets.data" "$OUT/index.html"
cp "$TEMPLATE/k2d_runner.js" "$TEMPLATE/k2d_runner.wasm" "$OUT/"

# Scene paths stay logical (.py), but only the precompiled bundle is staged.
(cd "$PROJECT" && tar --exclude='*.py' --exclude='.k2d/web' -cf - .) | (cd "$STAGE" && tar -xf -)
mapfile -t scripts < <(cd "$PROJECT" && find . -type f -name '*.py' -print | LC_ALL=C sort | sed 's|^./||')
if (( ${#scripts[@]} > 0 )); then
    (cd "$PROJECT" && "$ROOT/bin/k2d_scriptc" --bundle "$STAGE/.k2d/web/scripts.zbc" \
        "$STAGE/.k2d/web/scripts.json" "${scripts[@]}")
fi

FILE_PACKAGER="$(dirname "$(command -v emcc)")/tools/file_packager"
"$FILE_PACKAGER" "$OUT/assets.data" --preload "$STAGE@/project" --js-output="$OUT/assets.js" --export-name=Module --no-node --quiet

title="$(python3 - "$PROJECT/project.k2dproj" "$GAME" <<'PY'
import json, sys
p, fallback = sys.argv[1:]
data = json.load(open(p, encoding='utf8'))
print(data.get('name') or fallback)
PY
 )"
if [[ -z "$SCENE" ]]; then
scene="$(python3 - "$PROJECT/project.k2dproj" <<'PY'
import json, sys
data = json.load(open(sys.argv[1], encoding='utf8'))
print(data.get('startupScene') or '')
PY
)"
else
    scene="$SCENE"
fi
if [[ -z "$scene" ]]; then
    echo "No scene was selected and project.k2dproj has no startupScene" >&2
    exit 1
fi
if [[ "$scene" == /* || "$scene" == *\\* || "$scene" == .. || "$scene" == ../* || "$scene" == */../* || ! -f "$PROJECT/$scene" ]]; then
    echo "Scene must be an existing path inside the project: $scene" >&2
    exit 1
fi
python3 - "$OUT/index.html" "$title" "$scene" <<'PY'
import html, pathlib, sys
out, title, scene = sys.argv[1:]
pathlib.Path(out).write_text(f'''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>{html.escape(title)}</title><style>html,body,canvas{{margin:0;width:100%;height:100%;background:#111}}canvas{{display:block;outline:0}}</style>
<canvas id="canvas" tabindex="0"></canvas><script>var Module={{canvas:document.getElementById('canvas'),arguments:['/project/{scene}','/project/project.k2dproj']}};</script>
<script src="assets.js"></script><script async src="k2d_runner.js"></script>''', encoding='utf8')
PY
echo "Exported Web game: $OUT/index.html"

if [[ "$RUN" == 1 ]]; then
    "$ROOT/bin/k2d_webserver" "$OUT" "$PORT" &
    server=$!
    trap 'kill "$server" 2>/dev/null || true; rm -rf "$STAGE"' EXIT
    xdg-open "http://127.0.0.1:$PORT/" >/dev/null 2>&1 || true
    wait "$server"
fi
