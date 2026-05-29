#!/bin/bash
set -e
cd "$(dirname "$0")"

HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "nogit")
VERSION=$(grep 'APP_VERSION = ' bom_tracker.cpp | head -1 | sed 's/.*"\(.*\)".*/\1/')
echo "Building bom-tracker v${VERSION} (${HASH})"

g++ -std=c++17 -O2 \
    -DBUILD_HASH="\"${HASH}\"" \
    -o bom-tracker bom_tracker.cpp \
    imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp \
    imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp \
    -I imgui -I imgui/backends \
    $(pkg-config --cflags --libs glfw3 gl) \
    -lsqlite3

echo "Done → bom-tracker v${VERSION} (${HASH})"

# Install to ~/.local/bin
mkdir -p ~/.local/bin
cp bom-tracker ~/.local/bin/bom-tracker
echo "Installed → ~/.local/bin/bom-tracker"
