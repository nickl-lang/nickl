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
    echo "    --emulator=<name>   Crosscompiling emulator"
    echo "    -h,--help           Display this message"
}

ARG_EMULATOR=

while [ $# -gt 0 ]; do
    case "$1" in
        --file=*)
            ARG_FILE="${1#*=}"
            ;;
        --exe=*)
            ARG_EXE="${1#*=}"
            ;;
        --emulator=*)
            ARG_EMULATOR="${1#*=}"
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

extract_region() {
    OUTPUT_MARKER_START=$1
    OUTPUT_MARKER_END=$2
    sed -n "/$OUTPUT_MARKER_START/,/$OUTPUT_MARKER_END/ {//d; p}" $ARG_FILE | sed -z '$ s/\n$//'
}

EXPECTED_OUTPUTX="$(extract_region "@output" "@endoutput"; echo x)"
EXPECTED_OUTPUT="${EXPECTED_OUTPUTX%x}"

EXPECTED_RETCODEX="$(extract_region "@retcode" "@endretcode"; echo x)"
EXPECTED_RETCODE="${EXPECTED_RETCODEX%x}"

runTest() {
    COMMAND=$1

    RESULTX="$(set +e; ($COMMAND; echo x$?) | tr -d '\r')"
    RETCODE=${RESULTX##*x}
    OUTPUT="${RESULTX%x*}"

    if [ "$RETCODE" -ne "${EXPECTED_RETCODE:-0}" ]; then
        echo "TEST FAILED:
    Expected return code: ${EXPECTED_RETCODE:-0}
    Actual   return code: $RETCODE"
        exit 1
    fi

    if [ "$EXPECTED_OUTPUT" != "$OUTPUT" ]; then
        echo "TEST FAILED:
    Expected output:
        \"$EXPECTED_OUTPUT\"
    Actual   output:
        \"$OUTPUT\""
        exit 1
    fi
}

runCommand() {
    COMMAND=$1
    echo "Running '$COMMAND'" >&2
    $ARG_EMULATOR $COMMAND
}

run() {
    runCommand "$ARG_EXE -k run $ARG_FILE"
}

compile() {
    OUT_FILE=$(basename $ARG_FILE | cut -d. -f1)_test_out
    runCommand "$ARG_EXE -k exe -o $OUT_FILE $ARG_FILE" &&
    runCommand "./$OUT_FILE"
}

runTest run
runTest compile

echo 'TEST PASSED'
