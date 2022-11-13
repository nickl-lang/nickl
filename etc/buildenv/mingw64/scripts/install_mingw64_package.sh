#!/bin/sh
set -e
PACKAGE=$1
cd /root
wget https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-$PACKAGE -O $PACKAGE
if echo $PACKAGE | grep 'zst$'; then
  zstd -d $PACKAGE
  rm $PACKAGE
  PACKAGE=${PACKAGE%.zst}
fi
tar xvf $PACKAGE -C /opt
rm $PACKAGE
