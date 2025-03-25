#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

DEFAULT_IMAGE="$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m)"
[ -z ${IMAGE+x} ] && IMAGE=$DEFAULT_IMAGE

CONFIG=$(cat "$DIR/config.json")

IMAGE_NAME=$(echo "$CONFIG" | jq -r --arg IMAGE "$IMAGE" '.images[] | select(startswith($IMAGE + ":"))')

IMAGES=$(echo "$CONFIG" | jq -r .images[] | cut -d: -f1 | xargs echo)

[ -z "$IMAGE_NAME" ] && {
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

TAG="$BASE_NAME-$IMAGE_NAME"
