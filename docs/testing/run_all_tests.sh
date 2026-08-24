#!/bin/bash
# Runs all Online Compiler API test scripts in sequence.
# Continues through all scripts and reports a combined result.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "$SCRIPT_DIR"

# Check Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "ERROR: python3 is required but not installed."
    exit 1
fi

# Check requests is installed
if ! python3 -c "import requests" 2>/dev/null; then
    echo "ERROR: 'requests' package is required. Install with: pip3 install requests"
    exit 1
fi

SCRIPTS=(
    "authentication_test.py"
    "code_execution_test.py"
    "question_management_test.py"
    "test_case_management_test.py"
    "submission_history_test.py"
    "platform_test.py"
)

FAILED=0

for script in "${SCRIPTS[@]}"; do
    if [[ ! -f "$script" ]]; then
        echo "ERROR: $script not found."
        exit 1
    fi
    echo ""
    echo "=============================================="
    echo " Running $script"
    echo "=============================================="
    python3 "$script"
    if [[ $? -ne 0 ]]; then
        FAILED=1
    fi
done

echo ""
echo "=============================================="
if [[ $FAILED -eq 0 ]]; then
    echo " All tests passed"
    echo "=============================================="
    exit 0
else
    echo " Some tests failed (see above)"
    echo "=============================================="
    exit 1
fi
