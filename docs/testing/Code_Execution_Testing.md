# Code Execution Testing Plan

## Pre Setup
1. Server running on `localhost:3000`.
2. Docker is available and the server can execute code in containers.
3. `requests` Python package installed.

## Test Cases

---

## A. Supported Languages

### Test Case 1: Valid C++ Execution
1. POST `/api/run` with `language=cpp` and a "Hello C++" program.
2. Verify status 200, `exitCode=0`, and `stdout` contains "Hello C++".

### Test Case 2: Valid Python Execution
1. POST `/api/run` with `language=python` and `print("Hello Python")`.
2. Verify status 200, `exitCode=0`, and `stdout` contains "Hello Python".

### Test Case 3: Valid JavaScript Execution
1. POST `/api/run` with `language=javascript` and `console.log("Hello JS")`.
2. Verify status 200, `exitCode=0`, and `stdout` contains "Hello JS".

---

## B. Validation and Errors

### Test Case 4: Invalid Language
1. POST `/api/run` with `language=ruby`.
2. Verify status 400.

### Test Case 5: Empty Code
1. POST `/api/run` with `language=python` and empty `code`.
2. Verify status 400 or a handled 200 with no output.

### Test Case 6: C++ Syntax Error
1. POST `/api/run` with `language=cpp` and invalid code.
2. Verify status 200, `exitCode` non-zero, and `stderr` is populated.

---

## C. Timeouts and Size Limits

### Test Case 7: Infinite Loop Times Out
1. POST `/api/run` with `language=python` and `while True: pass`.
2. Verify status 200 and `timedOut=true`.

### Test Case 8: Oversized Request Body
1. POST `/api/run` with a payload larger than 1 MB.
2. Verify status 413.

### Test Case 9: Large Output
1. POST `/api/run` with `language=python` and `print("A" * 10000)`.
2. Verify status 200 and `stdout` length is at least 10000.
