#!/usr/bin/env python3
"""Code execution tests for the Online Compiler /api/run endpoint."""

import sys
import time

import requests

from test_helpers import (
    BaseAPIClient, TestResult,
    BASE_URL, DEFAULT_ADMIN_EMAIL, DEFAULT_ADMIN_PASSWORD,
    GREEN, RED, YELLOW, RESET,
)


def run_code(client: BaseAPIClient, language: str, code: str, input_data: str = "") -> tuple[int, dict]:
    return client.request_json("POST", "/api/run", json={
        "language": language,
        "code": code,
        "input": input_data,
    })


def run_tests():
    results = TestResult()
    client = BaseAPIClient(BASE_URL)

    # Code execution does not require admin auth, but keep a session ready.
    client.admin_login()

    print(f"{YELLOW}{'='*60}{RESET}")
    print(f"CODE EXECUTION TESTS")
    print(f"{YELLOW}{'='*60}{RESET}\n")

    # 2.1 Valid C++
    status, data = run_code(client, "cpp", r"""#include <iostream>
int main() {
    std::cout << "Hello C++" << std::endl;
    return 0;
}""")
    results.record(
        "Valid C++ execution",
        status == 200 and data.get("exitCode") == 0 and "Hello C++" in (data.get("stdout") or ""),
        f"Status {status}: {data.get('error', '')}; stdout={data.get('stdout', '')[:80]}"
    )

    # 2.2 Valid Python
    status, data = run_code(client, "python", 'print("Hello Python")')
    results.record(
        "Valid Python execution",
        status == 200 and data.get("exitCode") == 0 and "Hello Python" in (data.get("stdout") or ""),
        f"Status {status}: {data.get('error', '')}; stdout={data.get('stdout', '')[:80]}"
    )

    # 2.3 Valid JavaScript
    status, data = run_code(client, "javascript", 'console.log("Hello JS")')
    results.record(
        "Valid JavaScript execution",
        status == 200 and data.get("exitCode") == 0 and "Hello JS" in (data.get("stdout") or ""),
        f"Status {status}: {data.get('error', '')}; stdout={data.get('stdout', '')[:80]}"
    )

    # 2.4 Invalid language
    status, data = run_code(client, "ruby", 'puts "Hello"')
    results.record(
        "Invalid language returns 400",
        status == 400,
        f"Status {status}: {data.get('error', '')}"
    )

    # 2.5 Empty code
    status, data = run_code(client, "python", "")
    results.record(
        "Empty code handled",
        status in (200, 400),
        f"Status {status}: {data.get('error', '')}"
    )

    # 2.6 Syntax error in C++
    status, data = run_code(client, "cpp", "int main() { this is not valid }")
    results.record(
        "C++ syntax error returns non-zero exit",
        status == 200 and data.get("exitCode") != 0,
        f"Status {status}: {data.get('error', '')}; exitCode={data.get('exitCode')}"
    )

    # 2.7 Infinite loop / timeout
    status, data = run_code(client, "python", "while True: pass")
    results.record(
        "Infinite loop times out",
        status == 200 and data.get("timedOut") is True,
        f"Status {status}: timedOut={data.get('timedOut')}"
    )

    # 2.8 Request body > 1 MB
    large_code = "x = '" + "A" * (2 * 1024 * 1024) + "'"
    status, data = run_code(client, "python", large_code)
    results.record(
        "Oversized request rejected",
        status == 413,
        f"Status {status}: {data.get('error', '')}"
    )

    # 2.9 Large output
    status, data = run_code(client, "python", 'print("A" * 10000)')
    results.record(
        "Large output handled",
        status == 200 and len(data.get("stdout", "")) >= 10000,
        f"Status {status}: stdout len={len(data.get('stdout', ''))}"
    )

    # Rate-limit sanity: wait before next call
    time.sleep(1)

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
