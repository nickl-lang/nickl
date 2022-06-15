#!/bin/sh
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
PROJ_ROOT=$(realpath $DIR/../../..)
$DIR/run-docker.sh $PROJ_ROOT/etc/scripts/build-linux.sh
