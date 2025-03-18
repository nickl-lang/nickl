#!/bin/sh

set -e

print_usage() {
  echo >&2 "Usage: $0 -n NAME -s OPTIONS -l LONGOPTIONS -- ARGS"
}

if [ "$__ARGPARSE_RECURSIVE" != 1 ]; then
  PARSED=$(__ARGPARSE_RECURSIVE=1 "$0" "$0" hn:o:l: help,name:,options:,longoptions: "$@") || {
    print_usage
    echo >&2 "Use --help for more info"
    exit 1
  }
  eval "$PARSED"
  eval set -- "$__EXTRA_ARGS"

  if [ "$HELP" = 1 ]; then
    print_usage
    exit
  fi
else
  NAME="$1"
  OPTIONS="$2"
  LONGOPTIONS="$3"
  shift 3
fi

emit() {
  echo "$*"
  if [ "$ARGPARSE_DEBUG" = 1 ]; then
    echo >&2 "DEBUG: $*"
  fi
}

emit "__ALL_ARGS=\"$*\""

PARSED=$(getopt -a -n "$NAME" -o "$OPTIONS" -l "$LONGOPTIONS" -- "$@")
if [ $? -ne 0 ]; then
  exit 1
fi

eval set -- "$PARSED"

get_short_idx() {
  i=0
  idx=0
  len=${#OPTIONS}
  while [ $i -lt "$len" ]; do
    char=$(echo "$OPTIONS" | cut -c$((i+1)))
    if [ "$char" = "$1" ]; then
      echo $idx
      break
    fi
    if [ "$char" != ":" ]; then
      idx=$((idx+1))
    fi
    i=$((i+1))
  done
}

get_long_by_idx() {
  IFS=','
  i=0
  for long in $LONGOPTIONS; do
    if [ $i -eq "$1" ]; then
      echo "$long"
      break
    fi
    i=$((i+1))
  done
  unset IFS
}

get_long_by_name() {
  IFS=','
  for long in $LONGOPTIONS; do
    if [ "${long%:}" = "$1" ]; then
      echo "$long"
      break
    fi
  done
  unset IFS
}

while :; do
  case "$1" in
    --)
      shift
      break
      ;;
    --*)
      name=$(get_long_by_name "${1#??}")
      var_name=$(echo "${name%:}" | tr '[:lower:]-' '[:upper:]_')
      if [ "${name#"${name%?}"}" = ':' ]; then
        value="$2"
        emit "$var_name=\"$value\""
        shift 2
      else
        shift
        emit "$var_name=1"
      fi
      ;;
    -*)
      name=$(get_long_by_idx "$(get_short_idx "${1#?}")")
      var_name=$(echo "${name%:}" | tr '[:lower:]-' '[:upper:]_')
      if [ "${name#"${name%?}"}" = ':' ]; then
        value="$2"
        emit "$var_name=\"$value\""
        shift 2
      else
        emit "$var_name=1"
        shift
      fi
      ;;
    *)
      echo >&2 "ERROR: Invalid option: $1"
      exit 1
      ;;
  esac
done

if [ -n "$*" ]; then
  emit "__EXTRA_ARGS=\"$*\""
fi
