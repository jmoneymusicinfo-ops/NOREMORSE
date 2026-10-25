#!/usr/bin/env bash
# Requires LLVM 19 clang-cl/lld-link/llvm-rc/llvm-mt in PATH, CMake, Ninja,
# native installed JUCE 8.0.6, a prepared Microsoft SDK/CRT, and a Wine launcher.
set -euo pipefail
: "${JM_MSVC_ROOT:?Set to the prepared Microsoft toolchain root}"
: "${JM_MSVC_WINE_TOOLS:?Set to the mstorsjo/msvc-wine checkout}"
: "${JM_NATIVE_JUCE_PACKAGE:?Set to the native installed JUCE CMake package directory}"
: "${JM_WINE_RUNNER:?Set to the executable Wine launcher}"
cd "$(dirname "$0")/.."
JM_CROSS_JUCE_PACKAGE=$(python3 scripts/prepare-juce-cross-package.py "$JM_NATIVE_JUCE_PACKAGE")
BIN="$JM_MSVC_ROOT/bin/x64" source "$JM_MSVC_WINE_TOOLS/msvcenv-native.sh"
mkdir -p build-windows-clang
python3 scripts/windows-sdk-overlay.py "$PWD/build-windows-clang/windows-sdk-overlay.json"
JM_CASE_FLAGS="/clang:-ivfsoverlay /clang:$PWD/build-windows-clang/windows-sdk-overlay.json"
cmake -S . -B build-windows-clang -G Ninja \
 -DCMAKE_TOOLCHAIN_FILE=cmake/clang-windows-x64.cmake \
 -DJM_JUCE_PACKAGE_DIR="$JM_CROSS_JUCE_PACKAGE" \
 -DCMAKE_CROSSCOMPILING_EMULATOR="$JM_WINE_RUNNER" \
 -DCMAKE_C_FLAGS="$JM_CASE_FLAGS" -DCMAKE_CXX_FLAGS="$JM_CASE_FLAGS" \
 -DCMAKE_BUILD_TYPE=Release
cmake --build build-windows-clang --parallel 3
ctest --test-dir build-windows-clang --output-on-failure
