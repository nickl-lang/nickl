#!/bin/sh

set -e

printErrorUsage() {
    echo "See $(basename $0) --help for usage information" >&2
}

printUsage() {
    echo "Usage: $(basename $0) [options...]"
    echo "Options:"
    echo "    --file=<filepath>   Path to the test file"
    echo "    --exe=<filepath>    Path to the nkirc executable"
    echo "    -h,--help           Display this message"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --file=*)
            ARG_FILE="${1#*=}"
            ;;
        --exe=*)
            ARG_EXE="${1#*=}"
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

if [ -z $ARG_FILE ]; then
    echo 'error: missing `file` argument' >&2
    printErrorUsage
    exit 1
fi

if [ -z $ARG_EXE ]; then
    echo 'error: missing `exe` argument' >&2
    printErrorUsage
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
