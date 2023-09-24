#!/bin/sh
set -e
if [ ! -d $HOME/.wine ]; then
    winetricks nocrashdialog
fi
