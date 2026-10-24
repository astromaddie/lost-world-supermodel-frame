#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
TEST_ROOT=$(mktemp -d)
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM
CXX=${CXX:-g++}

# Debian/Ubuntu names the pkg-config module SDL2_net; Homebrew's module is
# lowercase. Keep the test usable with both native dependency packages.
SDL_NET_PKG=
for candidate in SDL2_net sdl2_net; do
  if pkg-config --exists "$candidate"; then
    SDL_NET_PKG=$candidate
    break
  fi
done
if [ -z "$SDL_NET_PKG" ]; then
  printf '%s\n' 'SDL2_net pkg-config module is unavailable.' >&2
  exit 1
fi

# Keep the harness itself warning-strict. Project translation units use the
# production Makefiles/Rules.inc flags, which do not promote legacy warnings.
pkg-config --exists sdl2
set --
"$CXX" -std=c++17 -Wall -Wextra -Werror \
  -isystem "$ROOT/Src" -isystem "$ROOT/Src/OSD/SDL" -I "$ROOT/Src/Pkgs" \
  $(pkg-config --cflags sdl2 "$SDL_NET_PKG") \
  -c "$ROOT/Scripts/frame/test-lost-world-input-path.cpp" \
  -o "$TEST_ROOT/test-lost-world-input-path.o"
set -- "$TEST_ROOT/test-lost-world-input-path.o"

for source in \
  Src/Inputs/Inputs.cpp \
  Src/Inputs/Input.cpp \
  Src/Inputs/InputTypes.cpp \
  Src/Inputs/InputSystem.cpp \
  Src/Inputs/InputSource.cpp \
  Src/Inputs/MultiInputSource.cpp \
  Src/OSD/SDL/SDLInputSystem.cpp \
  Src/OSD/SDL/Thread.cpp \
  Src/OSD/Logger.cpp \
  Src/Util/NewConfig.cpp \
  Src/Util/Format.cpp; do
  object="$TEST_ROOT/$(basename "${source%.cpp}").o"
  "$CXX" -std=c++17 -O3 -Wall -DGLEW_STATIC -DSUPERMODEL_FRAME \
    -fsigned-char -ffp-contract=off \
    -I "$ROOT/Src" -I "$ROOT/Src/OSD" -I "$ROOT/Src/OSD/SDL" -I "$ROOT/Src/Pkgs" \
    $(pkg-config --cflags sdl2 "$SDL_NET_PKG") \
    -c "$ROOT/$source" -o "$object"
  set -- "$@" "$object"
done

"$CXX" "$@" $(pkg-config --libs sdl2 "$SDL_NET_PKG") -o "$TEST_ROOT/test-lost-world-input-path"

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$TEST_ROOT/test-lost-world-input-path"
