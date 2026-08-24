#!/usr/bin/env python3
"""Authentication and session tests for the Online Compiler admin API."""

import sys

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
    print(f"AUTHENTICATION TESTS")
    print(f"{YELLOW}{'='*60}{RESET}\n")

    # 1.1 Valid login
    status, data = client.request_json("POST", "/api/admin/login", json={
        "email": DEFAULT_ADMIN_EMAIL,
        "password": DEFAULT_ADMIN_PASSWORD,
    })
    results.record(
        "Valid admin login",
        status == 200 and data.get("success"),
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.2 Invalid password
    status, data = client.request_json("POST", "/api/admin/login", json={
        "email": DEFAULT_ADMIN_EMAIL,
        "password": "wrongpassword",
    })
    results.record(
        "Invalid password returns 401",
        status == 401,
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.3 Missing fields
    status, data = client.request_json("POST", "/api/admin/login", json={
        "email": DEFAULT_ADMIN_EMAIL,
    })
    results.record(
        "Missing password returns 400",
        status == 400,
        f"Status {status}: {data.get('error', '')}"
    )

    # Login again to establish a valid session for the next tests
    client.admin_login()

    # 1.4 Valid session
    status, data = client.request_json("GET", "/api/admin/session")
    results.record(
        "Session returns 200 with valid cookie",
        status == 200 and data.get("email") == DEFAULT_ADMIN_EMAIL,
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.5 Protected route without auth
    no_auth_client = BaseAPIClient(BASE_URL)
    status, data = no_auth_client.request_json("GET", "/api/questions")
    results.record(
        "Questions route 401 without session",
        status == 401,
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.6 Logout
    status, data = client.request_json("POST", "/api/admin/logout")
    results.record(
        "Logout returns 200",
        status == 200 and data.get("success"),
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.7 Session invalid after logout
    status, data = client.request_json("GET", "/api/admin/session")
    results.record(
        "Session 401 after logout",
        status == 401,
        f"Status {status}: {data.get('error', '')}"
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
