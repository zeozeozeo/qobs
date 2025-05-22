#!/bin/bash

# Default path to qobs executable, can be overridden by first argument
QOBS_EXECUTABLE_DEFAULT="../cmake-build-debug/qobs"
QOBS_EXECUTABLE="${1:-$QOBS_EXECUTABLE_DEFAULT}"

# Check if the qobs executable exists
if [ ! -f "$QOBS_EXECUTABLE" ]; then
    echo "ERROR: qobs executable not found at $QOBS_EXECUTABLE"
    echo "Please build qobs first or provide the correct path as an argument."
    exit 1
fi
echo "Using qobs executable: $QOBS_EXECUTABLE"
echo ""


# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
echo "Running tests from directory: $SCRIPT_DIR"
echo ""

# Find all test scripts in the current directory (tests/)
TEST_SCRIPTS=$(find "$SCRIPT_DIR" -maxdepth 1 -name "test_*.sh" -not -name "$(basename "${BASH_SOURCE[0]}")" | sort)

PASSED_COUNT=0
FAILED_COUNT=0
TOTAL_COUNT=0

for test_script in $TEST_SCRIPTS; do
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    TEST_BASENAME=$(basename "$test_script")
    echo "----------------------------------------------------------------------"
    echo "EXECUTING TEST: $TEST_BASENAME"
    echo "----------------------------------------------------------------------"
    
    # Execute the test script, passing the qobs executable path
    # The subshell ensures that 'cd' operations within the test script don't affect this runner script.
    (
      if bash "$test_script" "$QOBS_EXECUTABLE"; then
          echo ""
          echo "✅ ✅ ✅ TEST SUCCEEDED: $TEST_BASENAME ✅ ✅ ✅"
          PASSED_COUNT=$((PASSED_COUNT + 1))
      else
          echo ""
          echo "❌ ❌ ❌ TEST FAILED: $TEST_BASENAME ❌ ❌ ❌"
          FAILED_COUNT=$((FAILED_COUNT + 1))
      fi
    )
    # Capture exit code of the subshell
    test_exit_code=$? 
    if [ $test_exit_code -ne 0 ] && [ $FAILED_COUNT -eq $((TOTAL_COUNT - PASSED_COUNT -1)) ]; then
        # This means the script itself errored out (e.g. set -e caused exit)
        # and we haven't already marked it as failed based on its own output.
        # However, the trap in test scripts should handle their own "FAIL" message.
        # This is more of a fallback.
        # If the script explicitly exits with 0 after printing FAIL, this won't catch it.
        # The current test scripts are designed to `exit 1` on failure.
        if [ $test_exit_code -ne 0 ]; then # If the script exited with non-zero
             if ! tail -n 5 "$test_script_output_tmp" | grep -q "FAIL:"; then # And it didn't print FAIL itself
                 echo ""
                 echo "❌ ❌ ❌ TEST SCRIPT ERRORED (or set -e exit): $TEST_BASENAME (Exit code: $test_exit_code) ❌ ❌ ❌"
                 # FAILED_COUNT might have been incremented by the script's own error path.
                 # This condition is tricky. The current test scripts should manage their FAIL state.
             fi
        fi
    fi

    echo "----------------------------------------------------------------------"
    echo ""
done

echo "======================================================================"
echo "TESTING SUMMARY"
echo "======================================================================"
echo "Total tests run: $TOTAL_COUNT"
echo "Passed: $PASSED_COUNT"
echo "Failed: $FAILED_COUNT"
echo "======================================================================"

if [ "$FAILED_COUNT" -ne 0 ]; then
    echo "Some tests failed."
    exit 1
else
    echo "All tests passed successfully!"
    exit 0
fi
