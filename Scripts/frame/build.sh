#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
IMAGE=lost-world-frame-build:local
BASE_IMAGE=ubuntu:22.04@sha256:b1066385161d28ddf6bc7e7b28a9170eec11484c821d1a5150d176cbde41d7f7

docker build --platform linux/arm64 -f "$ROOT/Scripts/frame/Dockerfile" -t "$IMAGE" "$ROOT/Scripts/frame"
BASE_IMAGE_ID=$(docker image inspect --format '{{.Id}}' "$BASE_IMAGE")
BUILD_IMAGE_ID=$(docker image inspect --format '{{.Id}}' "$IMAGE")

docker run --rm --platform linux/arm64 \
  -e FRAME_BASE_IMAGE_ID="$BASE_IMAGE_ID" \
  -e FRAME_BUILD_IMAGE_ID="$BUILD_IMAGE_ID" \
  -v "$ROOT:/src" -w /src "$IMAGE" \
  /bin/sh -ec 'make -f Makefiles/Makefile.Frame clean && make -j2 -f Makefiles/Makefile.Frame all'

docker run --rm --platform linux/arm64 \
  -e FRAME_BASE_IMAGE_ID="$BASE_IMAGE_ID" \
  -e FRAME_BUILD_IMAGE_ID="$BUILD_IMAGE_ID" \
  -v "$ROOT:/src" -w /src "$IMAGE" \
  /bin/sh Scripts/frame/package.sh

docker run --rm --platform linux/arm64 \
  -v "$ROOT:/src" -w /src "$IMAGE" \
  /bin/sh Scripts/frame/verify.sh
