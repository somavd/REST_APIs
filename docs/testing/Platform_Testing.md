# Platform Testing Plan

## Pre Setup
1. Server running on `localhost:3000`.
2. `requests` Python package installed.
3. A unique student email generated for each run to avoid collisions.

## Test Cases

---

## A. Student Registration

### Test Case 1: New Student Registration
1. POST `/api/student/register` with a unique `email`, `password`, and `name`.
2. Verify status 200 and JSON `success=true`.

### Test Case 2: Duplicate Registration
1. POST `/api/student/register` with the same `email` again.
2. Verify status 409 and an error message.

---

## B. Student Login and Session

### Test Case 3: Valid Student Login
1. POST `/api/student/login` with the registered `email` and `password`.
2. Verify status 200 and JSON `success=true` with `email` returned.

### Test Case 4: Student Session
1. GET `/api/student/session` with the `oc_student_session` cookie.
2. Verify status 200 and JSON contains `id`, `email`, and `name`.

---

## C. Platform Access Control

### Test Case 5: Unauthenticated Platform Request
1. GET `/api/platform/questions` without a session cookie.
2. Verify status 401.

### Test Case 6: List Platform Questions
1. Login as a student.
2. GET `/api/platform/questions`.
3. Verify status 200 and JSON contains a `questions` array.

### Test Case 7: Non-existent Question
1. GET `/api/platform/questions/99999`.
2. Verify status 404.

### Test Case 8: Submit to Non-existent Question
1. POST `/api/platform/submit` with `question_id=99999`, `language="python"`, and `code="print('hello')"`.
2. Verify status 404.

### Test Case 9: List Submissions
1. GET `/api/platform/submissions`.
2. Verify status 200 and JSON contains a `submissions` array.

---

## D. Student Logout

### Test Case 10: Valid Logout
1. POST `/api/student/logout`.
2. Verify status 200 and `success=true`.

### Test Case 11: Session After Logout
1. GET `/api/student/session` after logout.
2. Verify status 401.
