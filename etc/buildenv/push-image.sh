#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

. "$DIR/config.sh"

echo >&2 "INFO: Uploading docker image '$IMAGE'"

docker image tag $IMAGE $REMOTE/$IMAGE
docker image tag $IMAGE $REMOTE/$IMAGE_NAME:latest

docker push $REMOTE/$IMAGE
docker push $REMOTE/$IMAGE_NAME:latest

docker image rm $REMOTE/$IMAGE
docker image rm $REMOTE/$IMAGE_NAME:latest
