#!/bin/sh
DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)
"$DIR/vimsed" 'ggdj}dG' "$DIR/../../CHANGELOG.md"
