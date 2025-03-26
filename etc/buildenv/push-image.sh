#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

. "$DIR/config.sh"

echo >&2 "INFO: Uploading docker image '$TAG'"

docker image tag $TAG $REMOTE/$TAG
docker image tag $TAG $REMOTE/$IMAGE_NAME:latest

docker push $REMOTE/$TAG
docker push $REMOTE/$IMAGE_NAME:latest

docker image rm $REMOTE/$TAG
docker image rm $REMOTE/$IMAGE_NAME:latest
