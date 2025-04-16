#!/bin/sh
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)
"$DIR/vimsed" 'dj}dG' "$DIR/../../CHANGELOG.md"
