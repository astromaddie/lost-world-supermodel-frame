#!/bin/sh
set -eu

CALLER_DIR=$(pwd -P)
APP_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ -n "${XDG_DATA_HOME:-}" ]; then
  DATA_HOME=$XDG_DATA_HOME
else
  : "${HOME:?HOME must be set when XDG_DATA_HOME is unset}"
  DATA_HOME=$HOME/.local/share
fi
DATA_DIR=$DATA_HOME/lost-world-supermodel-frame

mkdir -p "$DATA_DIR/Config" "$DATA_DIR/Assets" "$DATA_DIR/ROMs"
for source_dir in Config Assets; do
  for source in "$APP_DIR/$source_dir"/*; do
    [ -f "$source" ] || continue
    name=${source##*/}
    target=$DATA_DIR/$source_dir/$name
    if [ ! -e "$target" ]; then
      cp -p "$source" "$target"
    fi
  done
done

cd "$DATA_DIR"

run_emulator()
{
  exec env LD_LIBRARY_PATH="$APP_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$APP_DIR/supermodel" "$@"
}

case "${1:-}" in
  --probe)
    shift
    seconds=30
    force_render=false
    seconds_set=false
    while [ "$#" -gt 0 ]; do
      case "$1" in
        --force-render)
          if [ "$force_render" = true ]; then
            printf '%s\n' 'Use --force-render only once.' >&2
            exit 2
          fi
          force_render=true
          shift
          ;;
        ''|*[!0-9]*)
          printf '%s\n' 'Usage: run-frame.sh --probe [SECONDS] [--force-render]' >&2
          exit 2
          ;;
        *)
          if [ "$seconds_set" = true ]; then
            printf '%s\n' 'Probe seconds may be specified only once.' >&2
            exit 2
          fi
          seconds=$1
          seconds_set=true
          shift
          ;;
      esac
    done
    case "$seconds" in ''|*[!0-9]*) printf '%s\n' 'Probe seconds must be an integer from 1 to 300.' >&2; exit 2 ;; esac
    if [ "$seconds" -lt 1 ] || [ "$seconds" -gt 300 ]; then
      printf '%s\n' 'Probe seconds must be from 1 to 300.' >&2
      exit 2
    fi
    if [ "$force_render" = true ]; then
      run_emulator -vr-probe "-vr-probe-seconds=$seconds" -window -res=496,384 -vr-probe-force-render
    fi
    run_emulator -vr-probe "-vr-probe-seconds=$seconds" -window -res=496,384
    ;;
  --flat)
    shift
    mode=flat
    ;;
  *)
    mode=vr
    ;;
esac

rom=${1:-}
if [ -n "$rom" ] && [ "${rom#-}" = "$rom" ]; then
  case "$rom" in
    /*) ;;
    *) rom_dir=$(CDPATH= cd -- "$CALLER_DIR/$(dirname -- "$rom")" 2>/dev/null && pwd) || {
         printf 'ROM path does not exist: %s\n' "$rom" >&2
         exit 1
       }
       rom=$rom_dir/$(basename -- "$rom") ;;
  esac
  shift
else
  rom=$DATA_DIR/ROMs/lostwsga.zip
fi

if [ ! -f "$rom" ]; then
  printf 'No ROM file found. Add %s/ROMs/lostwsga.zip or pass a ROM path.\n' "$DATA_DIR" >&2
  exit 1
fi

if [ "$mode" = flat ]; then
  run_emulator -no-vr -window -res=992,768 -no-wide-screen "$rom" "$@"
fi
run_emulator -vr -window -res=992,768 -no-wide-screen "$rom" "$@"
