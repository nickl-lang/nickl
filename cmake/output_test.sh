#!/bin/sh

set -e

printErrorUsage() {
  echo "See $(basename $0) --help for usage information" >&2
}

printUsage() {
  echo "Usage: $(basename $0) [options...]"
  echo "Options:"
  echo "    --file=<filepath>   Path to the test file"
  echo "    --cmd=<command>     Path to the tested executable"
  echo "    --args=<arguments>  Extra arguments to forward"
  echo "    -h,--help           Display this message"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --file=*)
      ARG_FILE="${1#*=}"
      ;;
    --cmd=*)
      ARG_CMD="${1#*=}"
      ;;
    --args=*)
      ARG_ARGS="${1#*=}"
      ;;
    -h|--help)
      printUsage
      exit 0
      ;;
    *)
      echo "error: invalid argument '$1'" >&2
      printErrorUsage
      exit 1
  esac
  shift
done

if [ -z "$ARG_FILE" ]; then
  echo 'error: missing `file` argument' >&2
  printErrorUsage
  exit 1
fi

if [ -z "$ARG_CMD" ]; then
  echo 'error: missing `cmd` argument' >&2
  printErrorUsage
  exit 1
fi

extract_region() {
  OUTPUT_MARKER_START=$1
  OUTPUT_MARKER_END=$2
  sed -n "/$OUTPUT_MARKER_START/,/$OUTPUT_MARKER_END/ {//d; p}" $ARG_FILE | sed -z '$ s/\n$//'
}

EXPECTED_OUTPUTX="$(extract_region "@output" "@endoutput"; echo x)"
EXPECTED_OUTPUT="${EXPECTED_OUTPUTX%x}"

ERROR_PATTERNX="$(extract_region "@error" "@enderror"; echo x)"
ERROR_PATTERN="${ERROR_PATTERNX%x}"

EXPECTED_RETCODEX="$(extract_region "@retcode" "@endretcode"; echo x)"
EXPECTED_RETCODE="${EXPECTED_RETCODEX%x}"

TMPDIR=
cleanup() {
  trap - EXIT
  if [ -n "$TMPDIR" ]; then rm -rf "$TMPDIR"; fi
  if [ -n "$1" ]; then trap - $1; kill -$1 $$; fi
}
TMPDIR=$(mktemp -dt nickl_output_test_XXXXXXXXXX)
trap 'cleanup' EXIT
trap 'cleanup HUP' HUP
trap 'cleanup TERM' TERM
trap 'cleanup INT' INT

STDOUT_FILE="$TMPDIR/actual_stdout"
STDERR_FILE="$TMPDIR/actual_stderr"

EXPECTED_STDOUT_FILE="$TMPDIR/expect_stdout"

printf "%s" "$EXPECTED_OUTPUT" > "$EXPECTED_STDOUT_FILE"

echo "Running command '$ARG_CMD $ARG_FILE'"

set +e
$ARG_CMD $ARG_FILE $ARG_ARGS 1>"$STDOUT_FILE" 2>"$STDERR_FILE"
RETCODE=$?
set -e

sed 's/\r//g' -i "$STDOUT_FILE"
sed 's/\r//g' -i "$STDERR_FILE"

OUTPUT_STDERRX="$(cat "$STDERR_FILE" | tee /dev/stderr; echo x)"
OUTPUT_STDERR="${OUTPUT_STDERRX%x}"

if [ "$RETCODE" -ne "${EXPECTED_RETCODE:-0}" ]; then
  printf "%s" "TEST FAILED:
Expected return code: ${EXPECTED_RETCODE:-0}
Actual   return code: $RETCODE"
  exit 1
fi

case "$OUTPUT_STDERR" in
  *"$ERROR_PATTERN"*)
    ;;
  *)
    printf "%s" "TEST FAILED:
  Error pattern:
      \"$ERROR_PATTERN\"
  Actual  error:
      \"$OUTPUT_STDERR\""
    exit 1
    ;;
esac

if ! diff --color=auto -u "$EXPECTED_STDOUT_FILE" "$STDOUT_FILE"; then
    echo '\nTEST FAILED'
    exit 1
fi

echo '\nTEST PASSED'
