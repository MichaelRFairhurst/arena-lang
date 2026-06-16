#!/bin/bash
TEST_FILE=$1
EXPECTED_FILE=$1.expected
COMPILER=$2

EXPECTED=$(cat "$EXPECTED_FILE")
ACTUAL=$($COMPILER "$TEST_FILE" 2>&1)
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