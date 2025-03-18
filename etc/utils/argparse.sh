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

  PARSED=$(__ARGPARSE_RECURSIVE=1 "$0" '-h,--help:-n,--name,NAME:-o,--options,OPTIONS' "$0" "$@") || {
    print_usage
    echo >&2 "Use --help for more info"
    exit 1
  }
  eval "$PARSED"
  eval set -- "$__POS_ARGS"

  if [ "$HELP" = 1 ]; then
    print_usage
    exit
  fi
fi

if [ -z "$NAME" ]; then
  NAME="$0"
fi

ORIG_ARGS=$*

OPTS=""
LONGOPTS=""
MAP=""

IFS=':'; for spec in $OPTIONS; do
  IFS=','; set -- $spec
  short_name="$1"
  long_name="$2"
  value_name="$3"
  OPTS="$OPTS${short_name#?}"
  if [ -n "$value_name" ]; then
    OPTS="$OPTS:"
  fi
  if [ -n "$long_name" ]; then
    var_name=$(echo "${long_name#??}" | tr '[:lower:]-' '[:upper:]_')
    if [ -n "$LONGOPTS" ]; then
      LONGOPTS="$LONGOPTS,"
    fi
    LONGOPTS="$LONGOPTS${long_name#??}"
    if [ -n "$value_name" ]; then
      LONGOPTS="$LONGOPTS:"
    fi
  else
    var_name=$(echo "${short_name#?}" | tr '[:lower:]-' '[:upper:]_')
  fi
  MAP="$MAP($short_name)($long_name) $var_name $value_name\n"
done

unset IFS

eval set -- $ORIG_ARGS
PARSED=$(getopt -a -n "$NAME" -o "$OPTS" -l "$LONGOPTS" -- "$@")
if [ $? -ne 0 ]; then
  exit 1
fi

eval set -- "$PARSED"

while :; do
  case "$1" in
    --)
      shift
      break
      ;;
    -*)
      spec=$(echo "$MAP" | grep -F "($1)")
      name=$(echo "$spec" | awk '{print $2}')
      value=$(echo "$spec" | awk '{print $3}')
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

echo "__ALL_ARGS='$ORIG_ARGS'"

if [ -n "$*" ]; then
  echo "__POS_ARGS=\"$*\""
fi
