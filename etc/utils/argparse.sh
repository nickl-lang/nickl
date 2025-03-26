#!/bin/sh

set -e

if [ "$ARGPARSE_SIMPLE" = 1 ]; then
  NAME="$1"
  OPTIONS="$2"
  shift 2
else
  print_usage() {
    echo >&2 "Usage: $0 [-n NAME] -o OPTIONS -- ARGS"
  }
  PARSED=$(ARGPARSE_SIMPLE=1 "$0" "$0" '-h,--help:-n,--name,=:-o,--options,=' "$@") || {
    print_usage
    echo >&2 "Use --help for more info"
    exit 1
  }
  eval "$PARSED"
  eval set -- "$__POS_ARGS"
  [ "$HELP" = 1 ] && {
    print_usage;
    echo >&2 "Options:
  -h, --help             Show this message
  -n, --name PROGNAME    Program name to show in error messages
  -o, --options SPECS    Colon-separated list of option specs

Spec examples:
  -o            -- Only short
  -o,,=         -- Only short with argument
  ,--option     -- Only long
  ,--option,=   -- Only long with argument
  -o,--option   -- Both
  -o,--option,= -- Both with argument
"
    exit;
  }
fi

NAME=${NAME:-"$0"}
ORIG_ARGS=$*

OPTS=''
LONGOPTS=''
MAP=''

OPTIONS=$(echo "$OPTIONS" | tr -d '[:space:]')
IFS=':'; for spec in $OPTIONS; do
  [ -z "$spec" ] && continue
  IFS=','; set -- $spec
  short="$1"
  long="$2"
  value="$3"
  [ -n "$short" ] && OPTS="$OPTS${short#?}${value:+:}"
  if [ -n "$long" ]; then
     var="${long#??}"
     LONGOPTS="${LONGOPTS:+$LONGOPTS,}${long#??}${value:+:}"
  else
     var="${short#?}"
  fi
  MAP="$MAP($short)($long) $(echo "$var" | tr '[:lower:]-' '[:upper:]_') $value\n"
done
unset IFS

if getopt >/dev/null 2>&1; then
  PARSED=$(getopt "$OPTS" $ORIG_ARGS) || exit 1
else
  PARSED=$(getopt -a -n "$NAME" -o "$OPTS" -l "$LONGOPTS" -- $ORIG_ARGS) || exit 1
fi
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
