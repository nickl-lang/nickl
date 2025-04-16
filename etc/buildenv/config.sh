#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

command -v docker >/dev/null 2>&1 || {
  echo >&2 'ERROR: docker is not installed, use `--native` to build without docker'
  exit 1
}

command -v jq >/dev/null 2>&1 || {
  echo >&2 'ERROR: jq is not installed, need it to parse docker buildenv config'
  exit 1
}

print_usage() {
  echo >&2 "Usage: $0 [-i IMAGE]"
}

DEFAULT_IMAGE="$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)"
CONFIG=$(cat "$DIR/config.json")
IMAGES=$(echo "$CONFIG" | jq -rc .images[] | cut -d: -f1 | xargs echo)

OPTIONS="
  -h, --help                        : Show this message
  -i, --image IMAGE=$DEFAULT_IMAGE    : Possible values: $IMAGES
"

PARSED=$(ARGPARSE_SIMPLE=1 "$DIR/../utils/argparse.sh" "$0" "$OPTIONS" "$@") || {
  print_usage
  echo >&2 "Use --help for more info"
  exit 1
}
eval "$PARSED"
eval set -- "$__POS_ARGS"

[ -z ${IMAGE+x} ] && IMAGE=$DEFAULT_IMAGE

IMAGE_CFG_TAG=$(echo "$CONFIG" | jq -rc --arg IMAGE "$IMAGE" '.images[] | select(startswith($IMAGE + ":"))')

[ "$HELP" = 1 ] && {
  print_usage
  echo >&2 "Options:$OPTIONS"
  exit
}

[ -z "$IMAGE_CFG_TAG" ] && {
  echo >&2 "ERROR: Invalid docker image '$IMAGE', possible images: $IMAGES"
  exit 1
}

SYSTEM=$(echo "$IMAGE" | cut -d- -f1)
MACHINE=$(echo "$IMAGE" | cut -d- -f2)

BASE_NAME=$(echo "$CONFIG" | jq -rc '.base_name')
REMOTE=$(echo "$CONFIG" | jq -rc '.remote')
ARGS=$(echo "$CONFIG" | jq -rc '.args | to_entries[]')
STAGES=$(echo "$CONFIG" | jq -rc '.stages[]')
DEBUG=$(echo "$CONFIG" | jq -rc '.debug_image_build')

IMAGE_NAME="$BASE_NAME-$(echo "$IMAGE_CFG_TAG" | cut -d: -f1)"
IMAGE_VERSION=$(echo "$IMAGE_CFG_TAG" | cut -d: -f2)

IMAGE_TAG="$IMAGE_NAME:$IMAGE_VERSION"

ARGS="$ARGS$(cat <<EOF

{"key":"SYSTEM","value":"$SYSTEM"}
{"key":"MACHINE","value":"$MACHINE"}
{"key":"IMAGE_BASE_NAME","value":"$BASE_NAME"}
{"key":"IMAGE_NAME","value":"$IMAGE_NAME"}
{"key":"IMAGE_VERSION","value":"$IMAGE_VERSION"}
{"key":"IMAGE_CREATED","value":"$(date -u +"%Y-%m-%dT%H:%M:%SZ")"}
EOF
)"
