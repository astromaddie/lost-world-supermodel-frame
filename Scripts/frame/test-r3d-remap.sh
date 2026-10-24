#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
TEST_ROOT=$(mktemp -d)
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM
TEST_BINARY=${SM_R3D_REMAP_TEST_BINARY:-"$TEST_ROOT/test-r3d-remap"}
# The legacy config templates and R3DShader override parameters are intentionally unused.
c++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter -I "$ROOT/Src" -I "$ROOT/Src/OSD/SDL" \
  "$ROOT/Scripts/frame/test-r3d-remap.cpp" \
  "$ROOT/Src/Graphics/New3D/R3DShader.cpp" "$ROOT/Src/Graphics/New3D/Mat4.cpp" \
  "$ROOT/Src/Util/NewConfig.cpp" "$ROOT/Src/Util/Format.cpp" \
  $(pkg-config --cflags --libs sdl2 glew) -lGL -o "$TEST_BINARY"
xvfb-run -a "$TEST_BINARY" "${SM_R3D_TEST_SIZE:-128}"
