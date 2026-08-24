#!/usr/bin/env python3
"""Test case management tests for the Online Compiler admin API."""

import sys
import time

import requests

from test_helpers import (
    BaseAPIClient, TestResult,
    BASE_URL, DEFAULT_ADMIN_EMAIL, DEFAULT_ADMIN_PASSWORD,
    GREEN, RED, YELLOW, RESET,
)


class TestCaseClient(BaseAPIClient):
    def create_question(self, title: str) -> tuple[int, dict]:
        return self.request_json("POST", "/api/questions", json={"title": title})

    def list_test_cases(self, question_id: int) -> tuple[int, dict]:
        return self.request_json("GET", f"/api/questions/{question_id}/testcases")

    def add_test_case(self, question_id: int, input_data: str, expected_output: str, is_hidden: bool) -> tuple[int, dict]:
        return self.request_json("POST", f"/api/questions/{question_id}/testcases", json={
            "input": input_data,
            "expected_output": expected_output,
            "is_hidden": is_hidden,
        })

    def delete_test_case(self, test_case_id: int) -> tuple[int, dict]:
        return self.request_json("DELETE", f"/api/testcases/{test_case_id}")

    def list_questions(self) -> tuple[int, dict]:
        return self.request_json("GET", "/api/questions")


def cleanup(client: TestCaseClient):
    status, data = client.list_questions()
    if status == 200:
        for q in data.get("questions", []):
            if q.get("title", "").startswith("TESTCASE_"):
                client.request_json("DELETE", f"/api/questions/{q['id']}")


def run_tests():
    results = TestResult()
    client = TestCaseClient(BASE_URL)

    print(f"{YELLOW}{'='*60}{RESET}")
    print(f"TEST CASE MANAGEMENT TESTS")
    print(f"{YELLOW}{'='*60}{RESET}\n")

    if not client.admin_login():
        print(f"{RED}Admin login failed. Cannot proceed.{RESET}")
        return False

    cleanup(client)
    time.sleep(0.5)

    # Create a question to attach test cases
    status, data = client.create_question("TESTCASE_Host Question")
    q_id = data.get("id")
    results.record(
        "Host question created",
        status == 200 and q_id is not None,
        f"Status {status}: {data.get('error', '')}"
    )

    if not q_id:
        print(f"{RED}No question id, cannot proceed.{RESET}")
        return False

    # 4.1 Add public test case
    status, data = client.add_test_case(q_id, "1 2", "3", False)
    tc_id = data.get("id")
    results.record(
        "Add public test case",
        status == 200 and tc_id is not None,
        f"Status {status}: {data.get('error', '')}"
    )

    # 4.2 Add hidden test case
    status, data = client.add_test_case(q_id, "5 5", "10", True)
    hidden_id = data.get("id")
    results.record(
        "Add hidden test case",
        status == 200 and hidden_id is not None,
        f"Status {status}: {data.get('error', '')}"
    )

    # 4.3 List test cases
    status, data = client.list_test_cases(q_id)
    results.record(
        "List test cases",
        status == 200 and "testcases" in data,
        f"Status {status}: {data.get('error', '')}"
    )

    if status == 200:
        cases = data.get("testcases", [])
        hidden = [c for c in cases if c.get("id") == hidden_id]
        if hidden:
            results.record(
                "Hidden test case flag stored",
                hidden[0].get("is_hidden") is True,
                f"is_hidden={hidden[0].get('is_hidden')}"
            )

    # 4.4 Delete test case
    if tc_id:
        status, data = client.delete_test_case(tc_id)
        results.record(
            "Delete test case",
            status == 200 and data.get("success"),
            f"Status {status}: {data.get('error', '')}"
        )

    # 4.5 Special characters in input/output
    status, data = client.add_test_case(q_id, "' OR 1=1", "<b>html</b>", False)
    results.record(
        "Special characters in test case",
        status == 200,
        f"Status {status}: {data.get('error', '')}"
    )
    if status == 200 and data.get("id"):
        client.delete_test_case(data["id"])

    # Cleanup
    print(f"\n{YELLOW}--- Cleanup ---{RESET}")
    client.request_json("DELETE", f"/api/questions/{q_id}")
    cleanup(client)
    print(f"{GREEN}Test case host questions cleaned up{RESET}")

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
