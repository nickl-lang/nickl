#!/bin/sh

set -e
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)

if command -v cmake >/dev/null 2>&2; then
  cmake -P "$DIR/GetMachineName.cmake"
else
  uname -m
fi
