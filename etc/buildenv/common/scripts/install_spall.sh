#!/bin/sh

set -e

SYSROOT=$1

URL="https://raw.githubusercontent.com/colrdavidson/spall-web/"
COMMIT=5129ce6778a9d732a7de727a79648a71d1092962

mkdir -p $SYSROOT/include
mkdir -p $SYSROOT/share/doc/spall

wget -O $SYSROOT/include/spall.h           "$URL/$COMMIT/spall.h"
wget -O $SYSROOT/share/doc/spall/AUTHORS   "$URL/$COMMIT/AUTHORS"
wget -O $SYSROOT/share/doc/spall/README.md "$URL/$COMMIT/README.md"
wget -O $SYSROOT/share/doc/spall/LICENSE   "$URL/$COMMIT/LICENSE"
