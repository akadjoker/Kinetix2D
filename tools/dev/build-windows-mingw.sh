#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
tools_root="${K2D_TOOLS_DIR:-${repo_root}/.k2d-tools}"
mingw_root="${K2D_MINGW_ROOT:-${tools_root}/mingw64}"
vcpkg_root="${K2D_VCPKG_ROOT:-${tools_root}/vcpkg}"
build_dir="${K2D_WINDOWS_BUILD_DIR:-${repo_root}/build-windows-mingw}"
mode="${1:-full}"

if [[ ! -x "${mingw_root}/bin/x86_64-w64-mingw32-g++" ]]; then
    echo "Missing portable MinGW-w64 SDK: ${mingw_root}" >&2
    echo "Expected: ${mingw_root}/bin/x86_64-w64-mingw32-g++" >&2
    exit 1
fi

toolchain="${repo_root}/cmake/toolchains/x86_64-w64-mingw32.cmake"

if [[ "${mode}" == "zen" ]]; then
    cmake -S "${repo_root}/external/zen/upstream/libzen" -B "${build_dir}-zen" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="${toolchain}" \
        -DK2D_MINGW_ROOT="${mingw_root}"
    cmake --build "${build_dir}-zen" --parallel
    exit 0
fi

if [[ "${mode}" != "full" ]]; then
    echo "Usage: $0 [full|zen]" >&2
    exit 2
fi

if [[ ! -x "${vcpkg_root}/vcpkg" ]]; then
    mkdir -p "${tools_root}"
    git clone --depth 1 https://github.com/microsoft/vcpkg.git "${vcpkg_root}"
    "${vcpkg_root}/bootstrap-vcpkg.sh" -disableMetrics
fi

cmake -S "${repo_root}" -B "${build_dir}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${vcpkg_root}/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="${toolchain}" \
    -DVCPKG_TARGET_TRIPLET=x64-mingw-static \
    -DK2D_MINGW_ROOT="${mingw_root}"

cmake --build "${build_dir}" --target k2d_editor k2d_runner k2d_pack k2d_kpak_tests --parallel

