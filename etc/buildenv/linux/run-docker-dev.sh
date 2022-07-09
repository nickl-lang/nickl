#!/bin/sh
set -e
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
. $DIR/image-info.sh
$DIR/run-docker-im.sh $IMAGE_TAG_DEV "$@"
