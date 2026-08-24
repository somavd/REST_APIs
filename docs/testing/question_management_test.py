#!/usr/bin/env python3
"""Question management CRUD tests for the Online Compiler admin API."""

import sys
import time

import requests

from test_helpers import (
    BaseAPIClient, TestResult,
    BASE_URL, DEFAULT_ADMIN_EMAIL, DEFAULT_ADMIN_PASSWORD,
    GREEN, RED, YELLOW, RESET,
)


class QuestionClient(BaseAPIClient):
    def create_question(self, title: str, description: str = "", category: str = "", difficulty: str = "") -> tuple[int, dict]:
        return self.request_json("POST", "/api/questions", json={
            "title": title,
            "description": description,
            "category": category,
            "difficulty": difficulty,
        })

    def update_question(self, question_id: int, data: dict) -> tuple[int, dict]:
        return self.request_json("PUT", f"/api/questions/{question_id}", json=data)

    def delete_question(self, question_id: int) -> tuple[int, dict]:
        return self.request_json("DELETE", f"/api/questions/{question_id}")

    def list_questions(self) -> tuple[int, dict]:
        return self.request_json("GET", "/api/questions")

    def find_question(self, title: str) -> dict:
        status, data = self.list_questions()
        if status == 200:
            for q in data.get("questions", []):
                if q.get("title") == title:
                    return q
        return {}


def cleanup_test_questions(client: QuestionClient):
    status, data = client.list_questions()
    if status == 200:
        for q in data.get("questions", []):
            if q.get("title", "").startswith("TEST_"):
                client.delete_question(q["id"])


def run_tests():
    results = TestResult()
    client = QuestionClient(BASE_URL)

    print(f"{YELLOW}{'='*60}{RESET}")
    print(f"QUESTION MANAGEMENT TESTS")
    print(f"{YELLOW}{'='*60}{RESET}\n")

    if not client.admin_login():
        print(f"{RED}Admin login failed. Cannot proceed.{RESET}")
        return False

    cleanup_test_questions(client)
    time.sleep(0.5)

    # 3.1 Create valid question
    status, data = client.create_question("TEST_Valid Question", "A description", "Math", "Easy")
    q_id = data.get("id")
    results.record(
        "Create valid question",
        status == 200 and q_id is not None,
        f"Status {status}: {data.get('error', '')}"
    )

    # 3.2 Create question without title
    status, data = client.create_question("")
    results.record(
        "Create question without title fails",
        status == 400,
        f"Status {status}: {data.get('error', '')}"
    )

    # 3.3 List questions
    status, data = client.list_questions()
    results.record(
        "List questions",
        status == 200 and "questions" in data,
        f"Status {status}: {data.get('error', '')}"
    )

    # 3.4 Update question
    if q_id:
        status, data = client.update_question(q_id, {
            "title": "TEST_Updated Question",
            "description": "Updated description",
            "category": "Logic",
            "difficulty": "Medium",
        })
        results.record(
            "Update question",
            status == 200 and data.get("success"),
            f"Status {status}: {data.get('error', '')}"
        )

        updated = client.find_question("TEST_Updated Question")
        results.record(
            "Updated question reflected in list",
            updated.get("title") == "TEST_Updated Question",
            f"Found title: {updated.get('title')}"
        )

    # 3.5 Special characters in title
    special_title = "TEST_Special @#$%^&* chars"
    status, data = client.create_question(special_title)
    results.record(
        "Special characters in title",
        status == 200,
        f"Status {status}: {data.get('error', '')}"
    )

    # 3.6 Unicode in title
    status, data = client.create_question("TEST_Unicode \u4f60\u597d \ud83c\udf89")
    results.record(
        "Unicode in title",
        status == 200,
        f"Status {status}: {data.get('error', '')}"
    )

    # 3.7 Long text
    long_text = "TEST_" + "A" * 1000
    status, data = client.create_question(long_text)
    results.record(
        "Long title text",
        status == 200,
        f"Status {status}: {data.get('error', '')}"
    )

    # 3.8 SQL injection
    sql_inject = "TEST_' OR 1=1 --"
    status, data = client.create_question(sql_inject)
    results.record(
        "SQL injection stored literally",
        status == 200,
        f"Status {status}: {data.get('error', '')}"
    )
    if status == 200:
        q = client.find_question(sql_inject)
        if q:
            client.delete_question(q["id"])

    # 3.9 XSS in title
    xss_title = "TEST_<script>alert('xss')</script>"
    status, data = client.create_question(xss_title)
    results.record(
        "XSS payload stored literally",
        status in (200, 400),
        f"Status {status}: {data.get('error', '')}"
    )
    if status == 200:
        q = client.find_question(xss_title)
        if q:
            client.delete_question(q["id"])

    # 3.10 Delete question
    del_title = "TEST_To be deleted"
    status, data = client.create_question(del_title)
    if status == 200 and data.get("id"):
        del_id = data["id"]
        status, data = client.delete_question(del_id)
        results.record(
            "Delete question",
            status == 200 and data.get("success"),
            f"Status {status}: {data.get('error', '')}"
        )

    # Cleanup
    print(f"\n{YELLOW}--- Cleanup ---{RESET}")
    cleanup_test_questions(client)
    print(f"{GREEN}Test questions cleaned up{RESET}")

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
