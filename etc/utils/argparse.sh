#!/bin/bash

print_usage() { echo >&2 "Usage: $0 [-n NAME] -o OPTIONS -- ARGS..."; }

print_error_usage() {
  print_usage
  echo >&2 "Use --help for more info"
  exit 1
}

# add ACCUMULATOR TOKEN
add() {
  local -n _ref=$1
  [ -n "$_ref" ] && _ref+=' '
  _ref+="'$2'"
}

# varname OPTNAME
varname() {
  local n=$1
  while [ "${n#-}" != "$n" ]; do n=${n#-}; done
  n=${n//-/_}
  printf '%s' "${n^^}"
}

# emit INDEX VALUE
emit() {
  printf "%s='%s'\n" "$(varname "${o_long[$1]:-${o_short[$1]}}")" "$2"
}

# err MSG
err() { echo >&2 "$name: $1"; exit 1; }

# lookup RESULT ARRAY KEY
lookup() {
  local -n _r=$1 _a=$2
  local _k=$3 j
  _r=-1
  for ((j = 0; j < ${#_a[@]}; j++)); do
    [ "${_a[j]}" = "$_k" ] && { _r=$j; return; }
  done
}

# consume_next DISPLAY
consume_next() {
  (( i < n )) || err "option '$1' requires an argument"
  val=${args[i]}; (( i++ )); add all "$val"
}

# parse_opt ARG
parse_opt() {
  local arg=$1 rest found val c nm haseq=0
  if [ "${arg#--}" != "$arg" ]; then
    rest=${arg#--}; c=${rest%%=*}
    [ "$rest" != "$c" ] && { haseq=1; val=${rest#*=}; }
    lookup found o_long "--$c"
    (( found >= 0 )) || err "invalid option '--$c'"
    nm=${o_long[found]:-${o_short[found]}}
    if [ "${o_val[found]}" = 1 ]; then
      (( haseq )) || consume_next "$nm"
      emit "$found" "$val"
    else
      (( haseq )) && err "option '$nm' doesn't allow an argument"
      emit "$found" 1
    fi
  else
    rest=${arg#-}
    while [ -n "$rest" ]; do
      c=${rest%"${rest#?}"}; rest=${rest#?}
      lookup found o_short "-$c"
      (( found >= 0 )) || err "invalid option '-$c'"
      if [ "${o_val[found]}" = 1 ]; then
        if [ -n "$rest" ]; then val=$rest; rest=''
        else consume_next "-$c"
        fi
        emit "$found" "$val"
      else
        emit "$found" 1
      fi
    done
  fi
}

# parse NAME SPEC ARG...
parse() {
  local name=$1 spec=$2
  shift 2
  local -a o_short=() o_long=() o_val=()
  local line def w s l v
  while IFS= read -r line; do
    def=${line%%:*}
    def=${def#"${def%%[![:space:]]*}"}
    [ -z "$def" ] && continue
    case $def in -*) ;; *) continue ;; esac
    def=${def//,/ }
    s='' l='' v=0
    for w in $def; do
      case $w in
        --*) l=$w ;;
        -*) s=$w ;;
        *) v=1 ;;
      esac
    done
    o_short+=("$s"); o_long+=("$l"); o_val+=("$v")
  done <<EOF
$spec
EOF

  local -a args=("$@")
  local n=${#args[@]} i=0 all='' pos='' arg
  while (( i < n )); do
    arg=${args[i]}; (( i++ ))
    add all "$arg"
    [ "$arg" = "--" ] && break
    case $arg in
      -?*) parse_opt "$arg" ;;
      *) add pos "$arg" ;;
    esac
  done
  while (( i < n )); do
    arg=${args[i]}; (( i++ ))
    add pos "$arg"; add all "$arg"
  done
  printf '__POS_ARGS="%s"\n' "$pos"
  printf '__ALL_ARGS="%s"\n' "$all"
}

if [ "$ARGPARSE_SIMPLE" = 1 ]; then
  parse "$@"
else
  META='
  -h, --help             : Show this message
  -n, --name PROGNAME    : Program name to show in error messages
  -o, --options SPECS    : List of option specs
'
  PARSED=$(parse "$0" "$META" "$@") || print_error_usage
  eval "$PARSED"
  eval set -- "$__POS_ARGS"
  if [ "$HELP" = 1 ]; then
    print_usage
    cat >&2 <<EOF
Options:$META
Spec examples:
  -o                  : Only short
  -o VAL              : Only short with argument
  --option            : Only long
  --option VAL        : Only long with argument
  -o, --option        : Both
  -o, --option VAL    : Both with argument
EOF
    exit 0
  fi
  [ -z "$OPTIONS" ] && print_error_usage
  NAME=${NAME:-"$0"}
  parse "$NAME" "$OPTIONS" "$@"
fi
