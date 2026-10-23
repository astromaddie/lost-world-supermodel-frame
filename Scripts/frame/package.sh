#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
BIN=$ROOT/binframe/supermodel
DIST=$ROOT/dist
NAME=lost-world-supermodel-frame-linux-arm64
PACKAGE=$DIST/$NAME
ARCHIVE=$DIST/$NAME.tar.gz
SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-0}
mkdir -p "$DIST"

STAGE_ROOT=$(mktemp -d "$DIST/.stage.XXXXXX")
STAGE=$STAGE_ROOT/$NAME
NEW_PACKAGE=$DIST/.$NAME.new.$$
TEMP_ARCHIVE=$DIST/.$NAME.archive.$$.tar.gz
BACKUP_PACKAGE=$DIST/.$NAME.backup.$$
BACKUP_ARCHIVE=$DIST/.$NAME.backup.$$.tar.gz
HAD_PACKAGE=0
HAD_ARCHIVE=0
PROMOTING=0
COMPLETE=0

cleanup()
{
  status=$?
  trap - EXIT HUP INT TERM
  if [ "$COMPLETE" -ne 1 ] && [ "$PROMOTING" -eq 1 ]; then
    if [ "$HAD_PACKAGE" -eq 1 ]; then
      if [ -e "$BACKUP_PACKAGE" ]; then
        rm -rf "$PACKAGE"
        mv "$BACKUP_PACKAGE" "$PACKAGE" || printf 'Could not restore previous package directory: %s\n' "$BACKUP_PACKAGE" >&2
      fi
    else
      rm -rf "$PACKAGE"
    fi
    if [ "$HAD_ARCHIVE" -eq 1 ]; then
      if [ -e "$BACKUP_ARCHIVE" ]; then
        rm -f "$ARCHIVE"
        mv "$BACKUP_ARCHIVE" "$ARCHIVE" || printf 'Could not restore previous package archive: %s\n' "$BACKUP_ARCHIVE" >&2
      fi
    else
      rm -f "$ARCHIVE"
    fi
  fi
  rm -rf "$STAGE_ROOT" "$NEW_PACKAGE"
  rm -f "$TEMP_ARCHIVE"
  if [ "$COMPLETE" -eq 1 ]; then
    rm -rf "$BACKUP_PACKAGE"
    rm -f "$BACKUP_ARCHIVE"
  fi
  exit "$status"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

verify_metadata()
{
  python3 - "$1" <<'PY'
import json, pathlib, sys
metadata = json.loads(pathlib.Path(sys.argv[1]).read_text())
if metadata.get("format") != 1 or metadata.get("target") != "aarch64-linux-gnu" or metadata.get("roms_or_user_assets_included") is not False:
    raise SystemExit("Artifact is not a generated ROM-free Frame ARM64 package")
PY
}

if [ -e "$PACKAGE" ]; then
  HAD_PACKAGE=1
  if [ ! -d "$PACKAGE" ] || [ ! -f "$PACKAGE/BUILD-METADATA.json" ] || [ ! -f "$PACKAGE/SHA256SUMS" ]; then
    printf 'Refusing to replace an unrecognized package path: %s\n' "$PACKAGE" >&2
    exit 1
  fi
  verify_metadata "$PACKAGE/BUILD-METADATA.json"
  (cd "$PACKAGE" && sha256sum -c SHA256SUMS)
fi
if [ -e "$ARCHIVE" ]; then
  HAD_ARCHIVE=1
  if [ ! -f "$ARCHIVE" ] || ! gzip -t "$ARCHIVE" || ! tar -tzf "$ARCHIVE" | grep -Fx "$NAME/BUILD-METADATA.json" >/dev/null; then
    printf 'Refusing to replace an unrecognized or invalid package archive: %s\n' "$ARCHIVE" >&2
    exit 1
  fi
  tar -xOzf "$ARCHIVE" "$NAME/BUILD-METADATA.json" > "$STAGE_ROOT/previous-metadata.json"
  verify_metadata "$STAGE_ROOT/previous-metadata.json"
fi

if [ ! -x "$BIN" ]; then
  printf 'Missing native build output: %s\n' "$BIN" >&2
  exit 1
fi
mkdir -p "$STAGE/lib" "$STAGE/Config" "$STAGE/Assets" "$STAGE/THIRD-PARTY-NOTICES"
install -m 0755 "$BIN" "$STAGE/supermodel"
install -m 0755 Scripts/frame/run-frame.sh "$STAGE/run-frame.sh"
install -m 0644 Docs/STEAM_FRAME.md "$STAGE/README.md"
install -m 0644 Docs/VR_ARCHITECTURE.md "$STAGE/VR_ARCHITECTURE.md"
install -m 0644 Docs/LICENSE.txt "$STAGE/LICENSE.txt"

for file in Games.xml Music.xml; do
  install -m 0644 "Config/$file" "$STAGE/Config/$file"
done
for file in DIR.txt VR-MENU.txt VRMenuFont-OFL.txt VRMenuFont.ttf p1crosshair.bmp p2crosshair.bmp FrameArcadeGun.obj FrameArcadeGun.mtl FrameArcadeGun.json FrameArcadeGun-NOTICE.txt; do
  install -m 0644 "Assets/$file" "$STAGE/Assets/$file"
done

find_library()
{
  soname=$1
  path=$(ldconfig -p | awk -v name="$soname" '$1 == name && tolower($0) ~ /aarch64/ { print $NF; exit }')
  if [ -z "$path" ] || [ ! -f "$path" ]; then
    printf 'Missing ARM64 runtime library: %s\n' "$soname" >&2
    exit 1
  fi
  readlink -f "$path"
}

install -m 0755 "$(find_library libopenxr_loader.so.1)" "$STAGE/lib/libopenxr_loader.so.1"
install -m 0755 "$(find_library libjsoncpp.so.25)" "$STAGE/lib/libjsoncpp.so.25"
install -m 0755 "$(find_library libSDL2_net-2.0.so.0)" "$STAGE/lib/libSDL2_net-2.0.so.0"
install -m 0644 /usr/share/doc/libopenxr-loader1/copyright "$STAGE/THIRD-PARTY-NOTICES/OpenXR-loader-copyright.txt"
install -m 0644 /usr/share/doc/libjsoncpp25/copyright "$STAGE/THIRD-PARTY-NOTICES/jsoncpp-copyright.txt"
install -m 0644 /usr/share/doc/libsdl2-net-2.0-0/copyright "$STAGE/THIRD-PARTY-NOTICES/SDL2_net-copyright.txt"

for elf in "$STAGE/supermodel" "$STAGE"/lib/*.so.*; do
  header=$(readelf -h "$elf")
  if ! printf '%s\n' "$header" | grep -q 'Machine:.*AArch64'; then
    printf 'Package contains a non-AArch64 ELF: %s\n' "$elf" >&2
    exit 1
  fi
  if ! readelf --version-info "$elf" | python3 Scripts/frame/check-abi.py; then
    printf 'Package ELF requires a GLIBC/GLIBCXX version newer than the Frame ABI: %s\n' "$elf" >&2
    exit 1
  fi
  dependencies=$(LD_LIBRARY_PATH="$STAGE/lib" ldd "$elf")
  if printf '%s\n' "$dependencies" | grep -q 'not found'; then
    printf 'Unresolved build-image dependency for %s:\n%s\n' "$elf" "$dependencies" >&2
    exit 1
  fi
done

python3 - "$ROOT" "$STAGE" "$SOURCE_DATE_EPOCH" <<'PY'
import hashlib, json, os, pathlib, subprocess, sys
root, stage, epoch = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), int(sys.argv[3])
tracked = subprocess.check_output(
    ["git", "ls-files", "-co", "--exclude-standard"], cwd=root, text=True
).splitlines()
source_hash = hashlib.sha256()
for rel in sorted(tracked):
    if rel.startswith(("objframe/", "binframe/", "dist/")):
        continue
    path = root / rel
    if not path.is_file():
        continue
    source_hash.update(rel.encode("utf-8") + b"\0")
    source_hash.update(hashlib.sha256(path.read_bytes()).digest())
def version(*args):
    try:
        return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT).splitlines()[0]
    except Exception as exc:
        return f"unavailable: {exc}"
files = []
for path in sorted(p for p in stage.rglob("*") if p.is_file()):
    rel = path.relative_to(stage).as_posix()
    if rel == "BUILD-METADATA.json":
        continue
    files.append({"path": rel, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
packages = subprocess.check_output(
    ["dpkg-query", "-W", "-f=${binary:Package}=${Version}\\n",
     "build-essential", "gcc-11", "g++-11", "make", "libsdl2-dev", "libsdl2-net-dev",
     "libopenxr-dev", "libopenxr-loader1", "libjsoncpp25", "libglu1-mesa-dev", "zlib1g-dev"],
    text=True
).splitlines()
metadata = {
    "format": 1,
    "target": "aarch64-linux-gnu",
    "distribution": "Ubuntu 22.04 ARM64 build container; SteamOS host runtime",
    "base_image_id": os.environ.get("FRAME_BASE_IMAGE_ID", "not supplied"),
    "build_image_id": os.environ.get("FRAME_BUILD_IMAGE_ID", "not supplied"),
    "compiler": version("g++-11", "--version"),
    "linker": version("ld", "--version"),
    "make": version("make", "--version"),
    "dependency_packages": packages,
    "jobs": 2,
    "frame_flags": ["-DSUPERMODEL_FRAME", "-fsigned-char", "-ffp-contract=off"],
    "source_revision": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
    "source_tree_sha256": source_hash.hexdigest(),
    "source_date_epoch": epoch,
    "roms_or_user_assets_included": False,
    "files": files,
}
runtime_sonames = set()
for path in [stage / "supermodel", *sorted((stage / "lib").glob("*.so.*"))]:
    output = subprocess.check_output(["ldd", str(path)], env={**os.environ, "LD_LIBRARY_PATH": str(stage / "lib")}, text=True)
    for line in output.splitlines():
        if "=>" in line:
            name = line.strip().split()[0]
            if name != "linux-vdso.so.1":
                runtime_sonames.add(name)
metadata["runtime_dependency_sonames"] = sorted(runtime_sonames)
(stage / "BUILD-METADATA.json").write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n")
PY

(
  cd "$STAGE"
  find . -type f ! -name SHA256SUMS -print | LC_ALL=C sort | xargs sha256sum > SHA256SUMS
)

python3 - "$STAGE" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
expected = {
    "supermodel", "run-frame.sh", "README.md", "VR_ARCHITECTURE.md", "LICENSE.txt",
    "BUILD-METADATA.json", "SHA256SUMS",
    "lib/libopenxr_loader.so.1", "lib/libjsoncpp.so.25", "lib/libSDL2_net-2.0.so.0",
    "Config/Games.xml", "Config/Music.xml",
    "Assets/DIR.txt", "Assets/VR-MENU.txt", "Assets/VRMenuFont-OFL.txt",
    "Assets/VRMenuFont.ttf", "Assets/p1crosshair.bmp", "Assets/p2crosshair.bmp",
    "Assets/FrameArcadeGun.obj", "Assets/FrameArcadeGun.mtl",
    "Assets/FrameArcadeGun.json", "Assets/FrameArcadeGun-NOTICE.txt",
    "THIRD-PARTY-NOTICES/OpenXR-loader-copyright.txt",
    "THIRD-PARTY-NOTICES/jsoncpp-copyright.txt",
    "THIRD-PARTY-NOTICES/SDL2_net-copyright.txt",
}
actual = {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file()}
if actual != expected:
    raise SystemExit(f"Package allowlist mismatch; extra={sorted(actual - expected)}, missing={sorted(expected - actual)}")
for rel in actual:
    lower = rel.lower()
    if any(marker in lower for marker in ("lostwsga", ".zip", ".nvram", ".nv", "vrgunmodel", "vrgamegun", "vrmenuframe")):
        raise SystemExit(f"Private game data or custom VR asset in package: {rel}")
PY

(cd "$STAGE" && sha256sum -c SHA256SUMS)
tar --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 --numeric-owner \
  -cf - -C "$STAGE_ROOT" "$NAME" | gzip -n > "$TEMP_ARCHIVE"
gzip -t "$TEMP_ARCHIVE"
mkdir "$STAGE_ROOT/archive-check"
tar -xzf "$TEMP_ARCHIVE" -C "$STAGE_ROOT/archive-check"
(cd "$STAGE_ROOT/archive-check/$NAME" && sha256sum -c SHA256SUMS)

PROMOTING=1
if [ -e "$PACKAGE" ]; then
  mv "$PACKAGE" "$BACKUP_PACKAGE"
fi
if [ -e "$ARCHIVE" ]; then
  mv "$ARCHIVE" "$BACKUP_ARCHIVE"
fi
PROMOTING=1
mv "$STAGE" "$NEW_PACKAGE"
mv "$NEW_PACKAGE" "$PACKAGE"
mv "$TEMP_ARCHIVE" "$ARCHIVE"
PROMOTING=0
COMPLETE=1
printf 'Package directory: %s\nArchive: %s\n' "$PACKAGE" "$ARCHIVE"
