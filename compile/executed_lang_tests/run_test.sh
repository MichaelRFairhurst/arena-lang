#!/bin/bash
TEST_FILE=$1
EXPECTED_FILE=$1.expected
STUBS_FILE="$(dirname "$TEST_FILE")/stubs.rna"
COMPILER=$2
OPT_LEVEL=$3

if [ ! -f "$STUBS_FILE" ]; then
    STUBS_FILE=""
fi

EXPECTED=$(cat "$EXPECTED_FILE")
ACTUAL=$($COMPILER run $STUBS_FILE "$TEST_FILE" $OPT_LEVEL 2>&1 ; echo -e "Program exited with code $?")
DIFF=$(echo "$ACTUAL" | diff -u "$EXPECTED_FILE" -)
EXIT_CODE=$?

if [ "$EXIT_CODE" -eq 0 ]; then
    exit 0
fi

for arg in "$@"; do
    if [[ "$arg" == "--update" ]]; then
        echo "Updating expected output for $TEST_FILE"
        echo "$ACTUAL" > "$EXPECTED_FILE"
        break
    fi
done

echo "FAILED: $TEST_FILE"
echo "Diff:"
echo "$DIFF"
exit 1