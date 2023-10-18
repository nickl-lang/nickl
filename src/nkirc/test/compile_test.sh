#!/bin/sh

set -xe

cleanup() {
    rm -rf ./${OUT_FILE}*
}

trap cleanup EXIT

$EMULATOR $COMPILER -o${OUT_FILE} $@
$EMULATOR ./${OUT_FILE}
