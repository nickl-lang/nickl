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

[ "$HELP" = 1 ] && {
  print_usage
  echo >&2 "Options:"
  echo >&2 "  -h, --help                          Show this message"
  echo >&2 "  -i, --image IMAGE=$DEFAULT_IMAGE    Possible values: $IMAGES"
  echo >&2 ""
  exit
}

DEFAULT_IMAGE="$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)"
[ -z ${IMAGE+x} ] && IMAGE=$DEFAULT_IMAGE

CONFIG=$(cat "$DIR/config.json")

IMAGE_CFG_TAG=$(echo "$CONFIG" | jq -r --arg IMAGE "$IMAGE" '.images[] | select(startswith($IMAGE + ":"))')

IMAGES=$(echo "$CONFIG" | jq -r .images[] | cut -d: -f1 | xargs echo)

[ -z "$IMAGE_CFG_TAG" ] && {
  echo >&2 "ERROR: Invalid docker image '$IMAGE', possible images: $IMAGES"
  exit 1
}

SYSTEM=$(echo $IMAGE | cut -d- -f1)
MACHINE=$(echo $IMAGE | cut -d- -f2)

BASE_NAME=$(echo "$CONFIG" | jq -r '.base_name')
REMOTE=$(echo "$CONFIG" | jq -r '.remote')
ARGS=$(echo "$CONFIG" | jq -r '.args | to_entries[]')
STAGES=$(echo "$CONFIG" | jq -r '.stages[]')
DEBUG=$(echo "$CONFIG" | jq -r '.debug_image_build')

ARGS="$ARGS$(cat <<EOF

{
  "key": "SYSTEM",
  "value": "$SYSTEM"
}
{
  "key": "MACHINE",
  "value": "$MACHINE"
}
EOF
)"

IMAGE_NAME="$BASE_NAME-$(echo $IMAGE_CFG_TAG | cut -d: -f1)"
IMAGE_VERSION=$(echo $IMAGE_CFG_TAG | cut -d: -f2)

TAG="$IMAGE_NAME:$IMAGE_VERSION"
