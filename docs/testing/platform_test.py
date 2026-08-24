#!/usr/bin/env python3
"""Platform (student) API tests for the Online Compiler coding platform."""

import sys
import uuid

import requests

from test_helpers import (
    BaseAPIClient, TestResult,
    BASE_URL,
    GREEN, RED, RESET, YELLOW,
)


def run_tests():
    results = TestResult()
    client = BaseAPIClient(BASE_URL)

    email = f"student-{uuid.uuid4()}@test.com"
    password = "testpass123"

    print(f"{YELLOW}{'='*60}{RESET}")
    print(f"PLATFORM TESTS")
    print(f"{YELLOW}{'='*60}\n{RESET}")

    # 1.1 Register new student
    status, data = client.request_json("POST", "/api/student/register", json={
        "email": email,
        "password": password,
        "name": "Test Student",
    })
    results.record(
        "Student registration",
        status == 200 and data.get("success"),
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.2 Duplicate registration
    status, data = client.request_json("POST", "/api/student/register", json={
        "email": email,
        "password": password,
        "name": "Test Student",
    })
    results.record(
        "Duplicate registration returns 409",
        status == 409,
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.3 Login
    status, data = client.request_json("POST", "/api/student/login", json={
        "email": email,
        "password": password,
    })
    results.record(
        "Student login",
        status == 200 and data.get("success") and data.get("email") == email,
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.4 Session
    status, data = client.request_json("GET", "/api/student/session")
    results.record(
        "Student session",
        status == 200 and data.get("email") == email,
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.5 Platform routes without auth
    no_auth = BaseAPIClient(BASE_URL)
    status, _ = no_auth.request_json("GET", "/api/platform/questions")
    results.record(
        "Platform questions 401 without session",
        status == 401,
        f"Status {status}"
    )

    # 1.6 List questions
    status, data = client.request_json("GET", "/api/platform/questions")
    results.record(
        "List platform questions",
        status == 200 and "questions" in data,
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.7 Non-existent question
    status, _ = client.request_json("GET", "/api/platform/questions/99999")
    results.record(
        "Non-existent question returns 404",
        status == 404,
        f"Status {status}"
    )

    # 1.8 Submit to non-existent question
    status, _ = client.request_json("POST", "/api/platform/submit", json={
        "question_id": 99999,
        "language": "python",
        "code": "print('hello')",
    })
    results.record(
        "Submit to non-existent question returns 404",
        status == 404,
        f"Status {status}"
    )

    # 1.9 Submission history
    status, data = client.request_json("GET", "/api/platform/submissions")
    results.record(
        "List platform submissions",
        status == 200 and "submissions" in data,
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.10 Logout
    status, data = client.request_json("POST", "/api/student/logout")
    results.record(
        "Student logout",
        status == 200 and data.get("success"),
        f"Status {status}: {data.get('error', '')}"
    )

    # 1.11 Session after logout
    status, _ = client.request_json("GET", "/api/student/session")
    results.record(
        "Session 401 after logout",
        status == 401,
        f"Status {status}"
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
