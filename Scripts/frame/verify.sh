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
printf '%s\n' 'Frame native verification passed (no active OpenXR runtime expected on builder).'
