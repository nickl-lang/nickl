#!/bin/sh

set -xe

OUT_FILE=test_export

cleanup() {
    rm -rf ./${OUT_FILE}*
}

trap cleanup EXIT

$EMULATOR $COMPILER -kobj -o${OUT_FILE}.o ${NKIR_SRC}
$EMULATOR gcc -g -O0 -otest_export ${OUT_FILE}.o $@
$EMULATOR ./${OUT_FILE}
