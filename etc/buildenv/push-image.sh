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

echo >&2 "INFO: Uploading docker image '$TAG'"

docker image tag $TAG $REMOTE/$TAG
docker image tag $TAG $REMOTE/$IMAGE_NAME:latest

docker push $REMOTE/$TAG
docker push $REMOTE/$IMAGE_NAME:latest

docker image rm $REMOTE/$TAG
docker image rm $REMOTE/$IMAGE_NAME:latest
