#!/bin/sh
set -eu

if [ "$#" -ne 1 ] || [ ! -d "$1" ]; then
  printf 'Usage: %s PACKAGE_DIRECTORY\n' "$0" >&2
  exit 2
fi

PACKAGE_DIR=$(CDPATH= cd -- "$1" && pwd)
TEST_ROOT=$(mktemp -d)
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM
APP=$TEST_ROOT/app
cp -R "$PACKAGE_DIR" "$APP"
cat > "$APP/supermodel" <<'STUB'
#!/bin/sh
printf '%s\n' "$@" > "$STUB_ARGS_FILE"
printf '%s\n' "${LD_LIBRARY_PATH:-}" > "$STUB_ENV_FILE"
STUB
chmod +x "$APP/supermodel"

DATA_HOME=$TEST_ROOT/data
export STUB_ARGS_FILE=$TEST_ROOT/args.txt STUB_ENV_FILE=$TEST_ROOT/env.txt
export XDG_DATA_HOME=$DATA_HOME HOME=$TEST_ROOT/home LD_LIBRARY_PATH=/system-test/lib
mkdir -p "$HOME"

"$APP/run-frame.sh" --probe 2 --force-render
grep -Fx -- '-vr-probe' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- '-vr-probe-seconds=2' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- '-res=496,384' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- '-vr-probe-force-render' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- "$APP/lib:/system-test/lib" "$STUB_ENV_FILE" >/dev/null
PERSIST=$DATA_HOME/lost-world-supermodel-frame
cmp "$APP/Config/Games.xml" "$PERSIST/Config/Games.xml"
cmp "$APP/Assets/VRMenuFont.ttf" "$PERSIST/Assets/VRMenuFont.ttf"
for asset in FrameArcadeGun.obj FrameArcadeGun.mtl FrameArcadeGun.json FrameArcadeGun-NOTICE.txt; do
  cmp "$APP/Assets/$asset" "$PERSIST/Assets/$asset"
done
printf 'private game gun\n' > "$PERSIST/Assets/VRGameGun.obj"
printf 'private custom gun\n' > "$PERSIST/Assets/VRGunModel.obj"

printf 'user config\n' > "$PERSIST/Config/Supermodel.ini"
printf 'user asset\n' > "$PERSIST/Assets/VRMenuFont.ttf"
cp "$STUB_ARGS_FILE" "$TEST_ROOT/probe-args.txt"
if "$APP/run-frame.sh" --probe 999 >/dev/null 2>&1; then
  printf '%s\n' 'Launcher accepted an out-of-range probe duration.' >&2
  exit 1
fi
cmp "$TEST_ROOT/probe-args.txt" "$STUB_ARGS_FILE"
if "$APP/run-frame.sh" --flat >/dev/null 2>&1; then
  printf '%s\n' 'Launcher started without a ROM.' >&2
  exit 1
fi
cmp "$TEST_ROOT/probe-args.txt" "$STUB_ARGS_FILE"
grep -Fx 'user config' "$PERSIST/Config/Supermodel.ini" >/dev/null
grep -Fx 'user asset' "$PERSIST/Assets/VRMenuFont.ttf" >/dev/null

mkdir -p "$TEST_ROOT/roms"
ROM="$TEST_ROOT/roms/Lost World test.zip"
: > "$ROM"
RELATIVE_ROM="roms/Lost World test.zip"
(cd "$TEST_ROOT" && "$APP/run-frame.sh" --flat "$RELATIVE_ROM" -show-fps)
grep -Fx -- '-no-vr' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- '-no-wide-screen' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- "$ROM" "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- '-show-fps' "$STUB_ARGS_FILE" >/dev/null
if grep -Fx -- '-vr' "$STUB_ARGS_FILE" >/dev/null; then
  printf '%s\n' 'Flat launcher unexpectedly enabled VR.' >&2
  exit 1
fi

"$APP/run-frame.sh" "$ROM" -vr-game-diagnostic -vr-game-diagnostic-yaw=90 -vr-game-diagnostic-seconds=75
grep -Fx -- '-vr' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- '-vr-game-diagnostic' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- '-vr-game-diagnostic-yaw=90' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- '-vr-game-diagnostic-seconds=75' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- "$ROM" "$STUB_ARGS_FILE" >/dev/null

mkdir -p "$PERSIST/ROMs"
cp "$ROM" "$PERSIST/ROMs/lostwsga.zip"
"$APP/run-frame.sh" -show-fps
grep -Fx -- '-vr' "$STUB_ARGS_FILE" >/dev/null
grep -Fx -- "$PERSIST/ROMs/lostwsga.zip" "$STUB_ARGS_FILE" >/dev/null
grep -Fx 'user config' "$PERSIST/Config/Supermodel.ini" >/dev/null
grep -Fx 'user asset' "$PERSIST/Assets/VRMenuFont.ttf" >/dev/null

grep -Fx 'private game gun' "$PERSIST/Assets/VRGameGun.obj" >/dev/null
grep -Fx 'private custom gun' "$PERSIST/Assets/VRGunModel.obj" >/dev/null
printf '%s\n' 'Frame launcher tests passed.'
