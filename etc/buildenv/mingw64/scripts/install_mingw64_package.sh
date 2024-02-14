#!/bin/sh
set -e
PACKAGE=$1
cd /root
curl -L https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-$PACKAGE -o $PACKAGE
tar xvf $PACKAGE -C /opt
rm $PACKAGE
