#!/bin/sh

set -e

if [ "$__ARGPARSE_RECURSIVE" = 1 ]; then
  OPTIONS="$1"
  NAME="$2"
  shift 2
else
  print_usage() {
    echo >&2 "Usage: $0 [-n NAME] -o OPTIONS -- ARGS"
  }
  PARSED=$(__ARGPARSE_RECURSIVE=1 "$0" '-h,--help:-n,--name,=:-o,--options,=' "$0" "$@") || {
    print_usage
    echo >&2 "Use --help for more info"
    exit 1
  }
  eval "$PARSED"
  eval set -- "$__POS_ARGS"
  [ "$HELP" = 1 ] && { print_usage; exit; }
fi

NAME=${NAME:-"$0"}
ORIG_ARGS=$*

OPTS=''
LONGOPTS=''
MAP=''

IFS=':'; for spec in $OPTIONS; do
  IFS=','; set -- $spec
  short="$1"
  long="$2"
  value="$3"
  if [ -n "$short" ]; then
    OPTS="$OPTS${short#?}${value:+:}"
  fi
  if [ -n "$long" ]; then
     var="${long#??}"
     LONGOPTS="${LONGOPTS:+$LONGOPTS,}${long#??}${value:+:}"
  else
     var="${short#?}"
  fi
  MAP="$MAP($short)($long) $(echo "$var" | tr '[:lower:]-' '[:upper:]_') $value\n"
done
unset IFS

PARSED=$(getopt -a -n "$NAME" -o "$OPTS" -l "$LONGOPTS" -- $ORIG_ARGS) || exit 1
eval set -- "$PARSED"

while :; do
  case "$1" in
    --) shift; break ;;
    -*)
      spec=$(echo "$MAP" | grep -F "($1)")
      name=$(echo "$spec" | cut -d' ' -f2)
      value=$(echo "$spec" | cut -d' ' -f3)
      if [ -n "$value" ]; then
        echo "$name='$2'"
        shift 2
      else
        echo "$name=1"
        shift
      fi
      ;;
  esac
done

[ -n "$ORIG_ARGS" ] && echo "__ALL_ARGS='$ORIG_ARGS'"
[ -n "$*" ] && echo "__POS_ARGS=\"$*\""

exit 0
