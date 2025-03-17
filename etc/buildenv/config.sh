#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

if [ -z ${TARGET+x} ]; then
  TARGET=$(uname -s | tr '[:upper:]' '[:lower:]')
fi

CONFIG=$(cat "$DIR/config.json")

BASE_NAME=$(echo "$CONFIG" | jq -r '.base_name')
REMOTE=$(echo "$CONFIG" | jq -r '.remote')
ARGS=$(echo "$CONFIG" | jq -r '.args | to_entries[]')
STAGES=$(echo "$CONFIG" | jq -r '.stages[]')
DEBUG=$(echo "$CONFIG" | jq -r '.debug_image_build')

IMAGE_CONFIG=$(echo "$CONFIG" | jq --arg TARGET "$TARGET" '.images[] | select(.image==$TARGET)')

if [ -z "$IMAGE_CONFIG" ]; then
  echo >&2 "ERROR: Invalid target '$TARGET'"
  echo "$CONFIG" | jq -r .images[].image | xargs echo >&2 "INFO: Possible targets:"
  exit 1
fi

TAG=$(echo "$IMAGE_CONFIG" | jq -r '.tag')

IMAGE="$BASE_NAME-$TARGET:$TAG"

if [ "$DEBUG" = true ]; then
  echo >&2 "DEBUG: BASE_NAME=$BASE_NAME"
  echo >&2 "DEBUG: TARGET=$TARGET"
  echo >&2 "DEBUG: TAG=$TAG"
fi
