#!/bin/sh

set -e

if [ "$ARGPARSE_SIMPLE" = 1 ]; then
  NAME="$1"
  OPTIONS="$2"
  shift 2
else
  OPTIONS_SPEC='
  -h, --help             : Show this message
  -n, --name PROGNAME    : Program name to show in error messages
  -o, --options SPECS    : List of option specs
'
  print_usage() {
    echo >&2 "Usage: $0 [-n NAME] -o OPTIONS -- ARGS"
  }
  PARSED=$(ARGPARSE_SIMPLE=1 "$0" "$0" "$OPTIONS_SPEC" "$@") || {
    print_usage
    echo >&2 "Use --help for more info"
    exit 1
  }
  eval "$PARSED"
  eval set -- "$__POS_ARGS"
  [ "$HELP" = 1 ] && {
    print_usage;
  echo >&2 "Options:$OPTIONS_SPEC
Spec examples:
  -o                  : Only short
  -o VAL              : Only short with argument
  --option            : Only long
  --option VAL        : Only long with argument
  -o, --option        : Both
  -o, --option VAL    : Both with argument
"
    exit;
  }
fi

NAME=${NAME:-"$0"}

quote_args() {
  for arg; do
    printf "'%s' " "$(printf "%s" "$arg" | sed "s/'/'\\\\''/g")"
  done
}
ORIG_ARGS="$(quote_args "$@")"

OPTS=''
LONGOPTS=''
MAP=''

while IFS= read -r line; do
  IFS=':'; set -- $line; line="$1"
  [ -z "$line" ] && continue
  IFS=','; set -- $line; field1="$1"; field2="$2"
  unset IFS
  set -- $field1; name1="$1"; val1="$2"
  set -- $field2; name2="$1"; val2="$2"
  if [ -z "$name2" ]; then
    case "$name1" in
      --*) short=''; long="$name1"; value="$val1" ;;
      *) short="$name1"; long=''; value="$val1" ;;
    esac
  else
    short="$name1"; long="$name2"; value="$val2"
  fi
  [ -n "$short" ] && OPTS="$OPTS${short#?}${value:+:}"
  if [ -n "$long" ]; then
     var="${long#??}"
     LONGOPTS="${LONGOPTS:+$LONGOPTS,}${long#??}${value:+:}"
  else
     var="${short#?}"
  fi
  MAP="$MAP($short)($long) $(echo "$var" | tr '[:lower:]-' '[:upper:]_') $value\n"
done <<EOF
$OPTIONS
EOF
unset IFS

eval "set -- $ORIG_ARGS"
if getopt >/dev/null 2>&1; then
  PARSED=$(getopt "$OPTS" "$@") || exit 1
else
  PARSED=$(getopt -a -n "$NAME" -o "$OPTS" -l "$LONGOPTS" -- "$@") || exit 1
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

[ -n "$ORIG_ARGS" ] && echo "__ALL_ARGS=\"$ORIG_ARGS\""
[ -n "$*" ] && echo "__POS_ARGS=\"$*\""

exit 0
