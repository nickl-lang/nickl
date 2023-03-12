#!/bin/sh

set -e

PACKAGE=$1
cd /root
wget https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-$PACKAGE -O $PACKAGE
tar xvf $PACKAGE -C /opt
rm $PACKAGE
