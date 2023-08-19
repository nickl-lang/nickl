#!/bin/sh

set -e

while [ $# -gt 0 ]; do
    case "$1" in
        --file=*)
            ARG_FILE="${1#*=}"
            ;;
        --exe=*)
            ARG_EXE="${1#*=}"
            ;;
        *)
            echo 'error: invalid arguments'
            exit 1
    esac
    shift
done

if [ -z $ARG_FILE ]; then
    echo 'error: missing `file` argument'
    exit 1
fi

extract_output() {
    OUTPUT_MARKER_START="@output"
    OUTPUT_MARKER_END="@endoutput"
    sed -n "/$OUTPUT_MARKER_START/,/$OUTPUT_MARKER_END/ {//d; p}" $ARG_FILE | sed -z '$ s/\n$//'
}

EXPECTED_OUTPUTX="$(extract_output; echo x)"
EXPECTED_OUTPUT="${EXPECTED_OUTPUTX%x}"

COMMAND="$ARG_EXE $ARG_FILE"
echo "Running '$COMMAND'"

RESULTX="$(set +e; $COMMAND; echo x$?)"
RETURNCODE=${RESULTX##*x}
OUTPUT="${RESULTX%x*}"

if [ "$EXPECTED_OUTPUT" != "$OUTPUT" ]; then
    echo "TEST FAILED:
Expected output:
    \"$EXPECTED_OUTPUT\"
Actual output:
    \"$OUTPUT\""
    exit 1
fi

if [ $RETURNCODE -ne 0 ]; then
    echo "TEST FAILED:
Command returned nonzero code"
    exit 1
fi

echo 'TEST PASSED'
