#!/usr/bin/env python3
"""Submission history tests for the Online Compiler /api/submissions endpoint."""

import sys
import time

import requests

from test_helpers import (
    BaseAPIClient, TestResult,
    BASE_URL, DEFAULT_ADMIN_EMAIL, DEFAULT_ADMIN_PASSWORD,
    GREEN, RED, YELLOW, RESET,
)


def run_tests():
    results = TestResult()
    client = BaseAPIClient(BASE_URL)

    print(f"{YELLOW}{'='*60}{RESET}")
    print(f"SUBMISSION HISTORY TESTS")
    print(f"{YELLOW}{'='*60}{RESET}\n")

    # 5.1 Get recent submissions
    status, data = client.request_json("GET", "/api/submissions?limit=10")
    results.record(
        "Get recent submissions",
        status == 200 and "submissions" in data,
        f"Status {status}: {data.get('error', '')}"
    )

    # 5.2 Limit parameter works
    status, data = client.request_json("GET", "/api/submissions?limit=3")
    submissions = data.get("submissions", [])
    results.record(
        "Limit parameter caps result count",
        status == 200 and len(submissions) <= 3,
        f"Status {status}; count={len(submissions)}"
    )

    # 5.3 Language filter works (assume at least one Python or C++ may exist)
    status, data = client.request_json("GET", "/api/submissions?limit=20&language=python")
    python_subs = data.get("submissions", [])
    results.record(
        "Language filter accepted",
        status == 200,
        f"Status {status}"
    )
    if status == 200 and python_subs:
        all_python = all(s.get("language") == "python" for s in python_subs)
        results.record(
            "Language filter returns only matching language",
            all_python,
            f"Non-python count={sum(1 for s in python_subs if s.get('language') != 'python')}"
        )

    # 5.4 Create a submission to ensure history has data
    status, data = client.request_json("POST", "/api/run", json={
        "language": "python",
        "code": "print('test submission')",
        "input": "",
    })
    time.sleep(0.5)

    results.record(
        "Create a submission",
        status == 200 and "exitCode" in data,
        f"Status {status}: {data.get('stdout', '')[:80]}{data.get('stderr', '')[:80]}"
    )

    # 5.5 Latest submission appears in history
    status2, data2 = client.request_json("GET", "/api/submissions?limit=1")
    if status2 == 200 and data2.get("submissions"):
        newest = data2["submissions"][0]
        results.record(
            "Latest submission appears in history",
            newest.get("code") == "print('test submission')",
            f"Found code: {newest.get('code', '')[:80]}"
        )

        # 5.6 Submission fields present
        required = {"id", "language", "code", "input", "stdout", "stderr", "exitCode", "timedOut", "createdAt"}
        missing = required - set(newest.keys())
        results.record(
            "Submission contains required fields",
            not missing,
            f"Missing fields: {missing}"
        )
    else:
        results.record(
            "Latest submission appears in history",
            False,
            f"Status {status2}: {data2.get('error', '')}"
        )
        results.record(
            "Submission contains required fields",
            False,
            "No submissions to verify"
        )

    return results.summary()


if __name__ == "__main__":
    try:
        success = run_tests()
        sys.exit(0 if success else 1)
    except requests.exceptions.ConnectionError:
        print(f"{RED}ERROR: Cannot connect to {BASE_URL}{RESET}")
        print(f"{RED}Make sure the server is running.{RESET}")
        sys.exit(1)
    except Exception as e:
        print(f"{RED}ERROR: {e}{RESET}")
        sys.exit(1)
