#!/usr/bin/env bash
# Builds VSL Performer.app on a Mac.
#
#   ./scripts/build-mac.sh          build
#   ./scripts/build-mac.sh --open   build and launch
#
# Needs: Xcode Command Line Tools (xcode-select --install) and CMake (brew install cmake).
set -euo pipefail
cd "$(dirname "$0")/.."

if ! command -v cmake >/dev/null; then
    echo "CMake is missing. Install Homebrew (https://brew.sh), then run: brew install cmake" >&2
    exit 1
fi
if ! xcode-select -p >/dev/null 2>&1; then
    echo "The Xcode Command Line Tools are missing. Run: xcode-select --install" >&2
    exit 1
fi

generator=()
if command -v ninja >/dev/null; then generator=(-G Ninja); fi

cmake -S . -B build "${generator[@]}" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure

app="build/app/VSLPerformerApp_artefacts/Release/VSL Performer.app"
echo
echo "Built: $app"
echo "Copy it to /Applications, or run with --open."

if [[ "${1:-}" == "--open" ]]; then
    open "$app"
fi
