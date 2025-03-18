#!/bin/sh

set -e

NAME="$1"
OPT="$2"
LONGOPT="$3"
shift 3

echo "__ALL_ARGS=\"$@\""

PARSED=$(getopt -a -n "$NAME" -o "$OPT" -l "$LONGOPT" -- "$@")
if [ $? -ne 0 ]; then
  exit 1
fi

eval set -- "$PARSED"

get_short_idx() {
  i=0
  len=${#OPT}
  while [ $i -lt "$len" ]; do
    char=$(echo $OPT | cut -c$((i+1)))
    if [ "$char" = "$1" ]; then
      echo $i
      break
    fi
    i=$((i+1))
  done
}

get_long_by_idx() {
  IFS=','
  i=0
  for long in $LONGOPT; do
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
  for long in $LONGOPT; do
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
        echo "$var_name=\"$value\""
        shift 2
      else
        shift
        echo "$var_name=1"
      fi
      ;;
    -*)
      name=$(get_long_by_idx "$(get_short_idx "${1#?}")")
      var_name=$(echo "${name%:}" | tr '[:lower:]-' '[:upper:]_')
      if [ "${name#"${name%?}"}" = ':' ]; then
        value="$2"
        echo "$var_name=\"$value\""
        shift 2
      else
        echo "$var_name=1"
        shift
      fi
      ;;
    *)
      echo >&2 "ERROR: Invalid option: $1"
      exit 1
      ;;
  esac
done

if [ -n "$@" ]; then
  echo "__EXTRA_ARGS=\"$@\""
fi
