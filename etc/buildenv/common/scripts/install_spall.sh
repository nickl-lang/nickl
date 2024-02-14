#!/bin/sh

set -e

SYSROOT=$1

URL="https://raw.githubusercontent.com/colrdavidson/spall-web/"
COMMIT=5129ce6778a9d732a7de727a79648a71d1092962

mkdir -p $SYSROOT/include
mkdir -p $SYSROOT/share/doc/spall

curl -L -o $SYSROOT/include/spall.h           "$URL/$COMMIT/spall.h"
curl -L -o $SYSROOT/share/doc/spall/AUTHORS   "$URL/$COMMIT/AUTHORS"
curl -L -o $SYSROOT/share/doc/spall/README.md "$URL/$COMMIT/README.md"
curl -L -o $SYSROOT/share/doc/spall/LICENSE   "$URL/$COMMIT/LICENSE"
