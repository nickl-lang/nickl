#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

. "$DIR/config.sh"

echo >&2 "INFO: Uploading docker image '$IMAGE_TAG'"

docker image tag $IMAGE_TAG $REMOTE/$IMAGE_TAG
docker image tag $IMAGE_TAG $REMOTE/$IMAGE_NAME:latest

docker push $REMOTE/$IMAGE_TAG
docker push $REMOTE/$IMAGE_NAME:latest

docker image rm $REMOTE/$IMAGE_TAG
docker image rm $REMOTE/$IMAGE_NAME:latest
