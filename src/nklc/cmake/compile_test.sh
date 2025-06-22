#!/bin/sh

set -xe

$EMULATOR $COMPILER -o${OUT_FILE} $@
$EMULATOR ./${OUT_FILE}
rm -f ./${OUT_FILE}
