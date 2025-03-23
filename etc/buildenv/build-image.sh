#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

print_usage() {
  echo >&2 "Usage: $0 [-i IMAGE]"
}

PARSED=$(ARGPARSE_SIMPLE=1 "$DIR/../utils/argparse.sh" "$0" '-h,--help:-i,--image,=' "$@") || {
  print_usage
  echo >&2 "Use --help for more info"
  exit 1
}
eval "$PARSED"
eval set -- "$__POS_ARGS"

. "$DIR/config.sh"

[ "$HELP" = 1 ] && {
  print_usage
  echo >&2 "Options:"
  echo >&2 "  -h, --help                          Show this message"
  echo >&2 "  -i, --image IMAGE=$DEFAULT_IMAGE    Possible values: $IMAGES"
  echo >&2 ""
  exit
}

echo >&2 "INFO: Building docker image '$IMAGE'"

maybe_tee() {
  if [ "$DEBUG" = true ]; then
    tee Dockerfile.debug
  else
    cat -
  fi
}

echo "$STAGES" | xargs -I{} cat "$DIR/src/Dockerfile.{}" |
  sed $(echo "$ARGS" | jq -r '. | "s@{{\(.key)}}@\(.value)@g"' | xargs -I{} echo -e "-e {}") |
  maybe_tee |
  docker build \
    -t "$TAG" \
    -f - \
    --target "$IMAGE" \
    "$@" \
    "$DIR/src"
