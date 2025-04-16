#!/bin/sh

set -e

command -v cmake >/dev/null 2>&2 && {
  BIN_DIR="$1"
  VAR="$2"

  cmake -L -N -B "$BIN_DIR" | grep "^$VAR" | cut -d '=' -f2
}
