#!/usr/bin/env bash
# Refresh the vendored libzen from a zenpy checkout.
#
#   external/zen/sync.sh [path-to-zenpy]        default: ../zenpy next to Kinetix2D
#
# Upstream is the source of truth: everything under include/ and src/ is
# overwritten. What this repo owns is kept — see UPSTREAM.md.

set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
zenpy="${1:-$(cd "$here/../../.." && pwd)/zenpy}"
upstream="$zenpy/libzen"

if [ ! -d "$upstream/src" ]; then
    echo "not a zenpy checkout: $upstream" >&2
    echo "usage: $0 [path-to-zenpy]" >&2
    exit 1
fi

# Ours, never taken from upstream.
keep_include=(zen_host_output.h)
keep_src=(zen_host_output.cpp)
# Upstream files this project deliberately does not build.
skip_src=(builtin_numpy.cpp)

skipped() {
    local name="$1"
    shift
    for entry in "$@"; do
        [ "$entry" = "$name" ] && return 0
    done
    return 1
}

copied=0
removed=0

for file in "$upstream/include/zen/"*; do
    name="$(basename "$file")"
    cp "$file" "$here/include/zen/$name"
    copied=$((copied + 1))
done

for file in "$upstream/src/"*; do
    name="$(basename "$file")"
    if skipped "$name" "${skip_src[@]}"; then
        continue
    fi
    cp "$file" "$here/src/$name"
    copied=$((copied + 1))
done

# Drop anything upstream deleted, without touching what is ours.
for file in "$here/include/zen/"*; do
    name="$(basename "$file")"
    skipped "$name" "${keep_include[@]}" && continue
    [ -f "$upstream/include/zen/$name" ] || { rm "$file"; removed=$((removed + 1)); }
done

for file in "$here/src/"*; do
    name="$(basename "$file")"
    skipped "$name" "${keep_src[@]}" && continue
    skipped "$name" "${skip_src[@]}" && continue
    [ -f "$upstream/src/$name" ] || { rm "$file"; removed=$((removed + 1)); }
done

commit="$(cd "$zenpy" && git rev-parse HEAD 2>/dev/null || echo unknown)"
branch="$(cd "$zenpy" && git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
dirty=""
if [ -n "$(cd "$zenpy" && git status --porcelain -- libzen 2>/dev/null)" ]; then
    dirty="  (WARNING: libzen has uncommitted changes upstream)"
fi

echo "synced $copied file(s), removed $removed"
echo "upstream $commit on $branch$dirty"
echo
echo "next: update the table in external/zen/UPSTREAM.md, rebuild, run the k2d_zen_* suites"
