#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

. "$DIR/config.sh"

echo >&2 "INFO: Building docker image '$IMAGE_TAG'"

maybe_tee() {
  if [ "$DEBUG" = true ]; then
    tee Dockerfile.debug
  else
    cat -
  fi
}

echo "$STAGES" | xargs -I{} cat "$DIR/src/Dockerfile.{}" |
  sed $(echo "$ARGS" | jq -r '. | "s@{{\(.key)}}@\(.value)@g"' | xargs -I{} echo "-e {}") |
  maybe_tee |
  docker build \
    -t "$IMAGE_TAG" \
    -f - \
    --target "$IMAGE" \
    "$@" \
    "$DIR/src"
