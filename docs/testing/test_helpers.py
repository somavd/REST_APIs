#!/usr/bin/env python3
"""
Shared test helpers for Online Compiler API test scripts.

Provides:
- Configuration constants (BASE_URL, admin credentials, default limits)
- ANSI color constants
- TestResult: pass/fail recorder with summary
- BaseAPIClient: session management, admin login, and JSON request helpers

Requires:
    pip install requests
"""

import requests

# Configuration
BASE_URL = "http://127.0.0.1:3000"
DEFAULT_ADMIN_EMAIL = "admin@example.com"
DEFAULT_ADMIN_PASSWORD = "admin123"
DEFAULT_ADMIN_NAME = "Administrator"

# Colors for output
GREEN = "\033[92m"
RED = "\033[91m"
YELLOW = "\033[93m"
RESET = "\033[0m"


class TestResult:
    """Accumulates pass/fail counts and prints a summary."""

    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.total = 0
        self.errors = []

    def record(self, test_name: str, passed: bool, error: str = ""):
        self.total += 1
        if passed:
            self.passed += 1
            print(f"{GREEN}\u2713 PASS{RESET}: {test_name}")
        else:
            self.failed += 1
            print(f"{RED}\u2717 FAIL{RESET}: {test_name}")
            if error:
                print(f"  {RED}Error: {error}{RESET}")
                self.errors.append(f"{test_name}: {error}")

    def summary(self) -> bool:
        print(f"\n{'='*60}")
        print(f"RESULTS: {GREEN}{self.passed} passed{RESET}, {RED}{self.failed} failed{RESET}, {self.total} total")
        print(f"{'='*60}")
        if self.errors:
            print(f"\n{RED}Failed tests:{RESET}")
            for err in self.errors:
                print(f"  - {err}")
        return self.failed == 0


class BaseAPIClient:
    """Base HTTP client with cookie-based admin session."""

    def __init__(self, base_url: str = BASE_URL):
        self.base_url = base_url
        self.session = requests.Session()
        self.admin_email = DEFAULT_ADMIN_EMAIL
        self.admin_password = DEFAULT_ADMIN_PASSWORD

    def request(self, method: str, path: str, **kwargs) -> requests.Response:
        url = f"{self.base_url}{path}"
        return self.session.request(method, url, **kwargs)

    def request_json(self, method: str, path: str, **kwargs) -> tuple[int, dict]:
        response = self.request(method, path, **kwargs)
        try:
            data = response.json() if response.text else {}
        except ValueError:
            data = {"raw": response.text}
        return response.status_code, data

    def admin_login(self, email: str = None, password: str = None) -> bool:
        email = email or self.admin_email
        password = password or self.admin_password
        response = self.session.post(
            f"{self.base_url}/api/admin/login",
            json={"email": email, "password": password}
        )
        return response.status_code == 200

    def admin_logout(self) -> bool:
        response = self.session.post(f"{self.base_url}/api/admin/logout")
        return response.status_code == 200
