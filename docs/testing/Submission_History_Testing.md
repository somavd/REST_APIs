# Submission History Testing Plan

## Pre Setup
1. Server running on `localhost:3000`.
2. At least one code execution submission exists in the database.
3. `requests` Python package installed.

## Test Cases

---

## A. Listing Submissions

### Test Case 1: Get Recent Submissions
1. GET `/api/submissions`.
2. Verify status 200 and JSON contains `submissions` array.

### Test Case 2: Limit Parameter
1. GET `/api/submissions?limit=3`.
2. Verify status 200 and the returned array length is at most 3.

### Test Case 3: Language Filter
1. GET `/api/submissions?limit=20&language=python`.
2. Verify status 200 and all returned submissions have `language=python`.

---

## B. Submission Fields

### Test Case 4: Required Fields Present
1. GET `/api/submissions?limit=1`.
2. Verify the first submission contains: `id`, `language`, `code`, `input`, `stdout`, `stderr`, `exitCode`, `timedOut`, `createdAt`.

---

## C. Create and Retrieve

### Test Case 5: New Submission Appears in History
1. POST `/api/run` with `language=python` and `print('test submission')`.
2. Wait briefly.
3. GET `/api/submissions?limit=1`.
4. Verify the newest submission has `code` matching the submitted code.
