#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
BIN=$ROOT/binframe/supermodel
PACKAGE=$ROOT/dist/lost-world-supermodel-frame-linux-arm64
TEST_ROOT=$(mktemp -d)
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM
mkdir -p "$TEST_ROOT/Config" "$TEST_ROOT/Assets"
cp Config/Games.xml Config/Music.xml "$TEST_ROOT/Config/"
cp Assets/DIR.txt Assets/VRMenuFont.ttf "$TEST_ROOT/Assets/"

(cd "$TEST_ROOT" && "$BIN" -help > help.txt)
grep -F -- '-vr-probe-force-render' "$TEST_ROOT/help.txt" >/dev/null
grep -F -- '-no-vr' "$TEST_ROOT/help.txt" >/dev/null
grep -F -- '-vr-game-diagnostic' "$TEST_ROOT/help.txt" >/dev/null
(cd "$TEST_ROOT" && "$BIN" -print-games > games.txt)
grep -i -F -- 'lost world' "$TEST_ROOT/games.txt" >/dev/null

if (cd "$TEST_ROOT" && "$BIN" -vr-probe -vr-probe-seconds=0 > invalid.txt 2>&1); then
  printf '%s\n' 'Probe accepted an out-of-range duration.' >&2
  exit 1
fi
grep -F -- 'requires an integer from 1 to 300' "$TEST_ROOT/invalid.txt" >/dev/null
if (cd "$TEST_ROOT" && "$BIN" -vr-probe-force-render > force-without-probe.txt 2>&1); then
  printf '%s\n' 'Probe force-render was accepted without probe mode.' >&2
  exit 1
fi
grep -F -- "requires '-vr-probe'" "$TEST_ROOT/force-without-probe.txt" >/dev/null
if (cd "$TEST_ROOT" && "$BIN" -vr-game-diagnostic-yaw=45 > invalid-yaw.txt 2>&1); then
  printf '%s\n' 'Game diagnostic accepted an unsupported yaw.' >&2
  exit 1
fi
grep -F -- 'requires 0, 90, or 180 degrees' "$TEST_ROOT/invalid-yaw.txt" >/dev/null
if (cd "$TEST_ROOT" && "$BIN" -vr-game-diagnostic-seconds=301 > invalid-game-timeout.txt 2>&1); then
  printf '%s\n' 'Game diagnostic accepted an out-of-range timeout.' >&2
  exit 1
fi
grep -F -- 'requires an integer from 1 to 300' "$TEST_ROOT/invalid-game-timeout.txt" >/dev/null
if (cd "$TEST_ROOT" && "$BIN" -vr-game-diagnostic-yaw=90 > diagnostic-param-without-mode.txt 2>&1); then
  printf '%s\n' 'Game diagnostic yaw was accepted without diagnostic mode.' >&2
  exit 1
fi
grep -F -- "options require '-vr-game-diagnostic'" "$TEST_ROOT/diagnostic-param-without-mode.txt" >/dev/null
if (cd "$TEST_ROOT" && "$BIN" -vr-game-diagnostic > diagnostic-no-rom.txt 2>&1); then
  printf '%s\n' 'Game diagnostic accepted a missing ROM.' >&2
  exit 1
fi
grep -F -- 'No ROM file specified.' "$TEST_ROOT/diagnostic-no-rom.txt" >/dev/null

mkdir -p "$TEST_ROOT/parser/Config" "$TEST_ROOT/parser/Assets"
cp "$PACKAGE/Config/Games.xml" "$PACKAGE/Config/Music.xml" "$TEST_ROOT/parser/Config/"
cp "$PACKAGE/Assets/"* "$TEST_ROOT/parser/Assets/"
printf '[ Video ]\nVR = 1\n' > "$TEST_ROOT/parser/Config/Supermodel.ini"
(cd "$TEST_ROOT/parser" && xvfb-run -a "$BIN" -no-vr -print-inputs \
  "-log-output=$TEST_ROOT/parser/runtime.log" > no-vr-config.txt 2>&1)
grep -E 'VR=(0|false)' "$TEST_ROOT/parser/runtime.log" >/dev/null

(cd "$TEST_ROOT" && xvfb-run -a "$BIN" -print-gl-info > gl-info.txt 2>&1)
grep -F -- 'OpenGL' "$TEST_ROOT/gl-info.txt" >/dev/null

set +e
(cd "$TEST_ROOT" && xvfb-run -a env XR_RUNTIME_JSON="$TEST_ROOT/no-runtime.json" \
  timeout 8 "$BIN" -vr-probe -vr-probe-seconds=1 > no-runtime.txt 2>&1)
probe_status=$?
set -e
if [ "$probe_status" -eq 0 ] || [ "$probe_status" -eq 124 ]; then
  printf 'No-runtime probe returned an invalid status: %s\n' "$probe_status" >&2
  cat "$TEST_ROOT/no-runtime.txt" >&2
  exit 1
fi
grep -E 'OpenXR|VR:' "$TEST_ROOT/no-runtime.txt" >/dev/null
if grep -F 'stereo projection frames=' "$TEST_ROOT/no-runtime.txt" >/dev/null; then
  printf '%s\n' 'No-runtime path unexpectedly reported a probe summary.' >&2
  exit 1
fi

Scripts/frame/test-launcher.sh "$ROOT/dist/lost-world-supermodel-frame-linux-arm64"
python3 "$ROOT/Scripts/frame/test-abi.py"
python3 "$ROOT/Scripts/frame/test-arcade-gun.py" "$PACKAGE/Assets"
g++ -std=c++17 -Wall -Wextra -Werror "$ROOT/Scripts/frame/test-frame-view-gesture.cpp" \
  -o "$TEST_ROOT/test-frame-view-gesture"
"$TEST_ROOT/test-frame-view-gesture"
g++ -std=c++17 -Wall -Wextra -Werror -I "$ROOT/Src/Pkgs" "$ROOT/Scripts/frame/test-controller-aim.cpp" \
  -o "$TEST_ROOT/test-controller-aim"
"$TEST_ROOT/test-controller-aim"
Scripts/frame/test-lost-world-input-path.sh
g++ -std=c++17 -Wall -Wextra -Werror -I "$ROOT/Src/Pkgs" "$ROOT/Scripts/frame/test-diagnostic-view-lock.cpp" \
  -o "$TEST_ROOT/test-diagnostic-view-lock"
"$TEST_ROOT/test-diagnostic-view-lock"
g++ -std=c++17 -Wall -Wextra -Werror "$ROOT/Scripts/frame/test-remap-diagnostic.cpp" \
  -o "$TEST_ROOT/test-remap-diagnostic"
"$TEST_ROOT/test-remap-diagnostic"
g++ -std=c++17 -Wall -Wextra -Werror "$ROOT/Scripts/frame/test-vr-face-normal.cpp" \
  -o "$TEST_ROOT/test-vr-face-normal"
"$TEST_ROOT/test-vr-face-normal"
Scripts/frame/test-r3d-composite.sh
Scripts/frame/test-r3d-remap.sh
Scripts/frame/test-r3d-empty-transparency.sh
printf '%s\n' 'Frame native verification passed (no active OpenXR runtime expected on builder).'
