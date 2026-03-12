#!/bin/sh
set -eu

if [ "$#" -lt 4 ]; then
    echo "Usage: build_user_dsp.sh <project_dir> <sdk_dir> <output.module> <debug_symbols_path>" >&2
    exit 1
fi

PROJECT_DIR="$1"
SDK_DIR="$2"
OUTPUT_MODULE="$3"
OUTPUT_DEBUG_SYMBOLS="$4"

if [ ! -d "$PROJECT_DIR" ]; then
    echo "Project directory not found: $PROJECT_DIR" >&2
    exit 1
fi

if [ ! -f "$SDK_DIR/UserDspApi.h" ]; then
    echo "User DSP SDK header not found in $SDK_DIR" >&2
    exit 1
fi

OUTPUT_DIR="$(dirname "$OUTPUT_MODULE")"
mkdir -p "$OUTPUT_DIR"

if [ -n "$OUTPUT_DEBUG_SYMBOLS" ]; then
    rm -rf "$OUTPUT_DEBUG_SYMBOLS"
fi

SOURCES_FILE="$(mktemp)"
find "$PROJECT_DIR" -type f -name '*.cpp' -print0 > "$SOURCES_FILE"

if [ ! -s "$SOURCES_FILE" ]; then
    rm -f "$SOURCES_FILE"
    echo "No .cpp source files were found in $PROJECT_DIR" >&2
    exit 1
fi

echo "Compiling project \"$PROJECT_DIR\""
xargs -0 xcrun clang++ \
    -std=c++20 \
    -dynamiclib \
    -fPIC \
    -g \
    -O0 \
    -Wall \
    -Wextra \
    -I"$SDK_DIR" \
    -I"$PROJECT_DIR" \
    -I"$PROJECT_DIR/include" \
    -o "$OUTPUT_MODULE" \
    < "$SOURCES_FILE"

rm -f "$SOURCES_FILE"

if [ -n "$OUTPUT_DEBUG_SYMBOLS" ] && xcrun -f dsymutil >/dev/null 2>&1; then
    xcrun dsymutil "$OUTPUT_MODULE" -o "$OUTPUT_DEBUG_SYMBOLS" >/dev/null 2>&1 || true
fi
