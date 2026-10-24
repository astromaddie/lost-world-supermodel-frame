#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
TEST_ROOT=$(mktemp -d)
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM
TEST_BINARY=${SM_R3D_EMPTY_TRANSPARENCY_TEST_BINARY:-"$TEST_ROOT/test-r3d-empty-transparency"}
c++ -std=c++17 -Wall -Wextra -Werror -I "$ROOT/Src" -I "$ROOT/Src/OSD/SDL" \
  "$ROOT/Scripts/frame/test-r3d-empty-transparency.cpp" \
  "$ROOT/Src/Graphics/New3D/R3DFrameBuffers.cpp" \
  "$ROOT/Src/Graphics/New3D/GLSLShader.cpp" \
  "$ROOT/Src/Graphics/New3D/Mat4.cpp" \
  $(pkg-config --cflags --libs sdl2 glew) -lGL -o "$TEST_BINARY"
xvfb-run -a "$TEST_BINARY" "${SM_R3D_TEST_SIZE:-128}"
