# Question Management Testing Plan

## Pre Setup
1. Server running on `localhost:3000`.
2. Admin is logged in and session cookie is set.
3. `requests` Python package installed.

## Test Cases

---

## A. Question Creation

### Test Case 1: Create Valid Question
1. POST `/api/questions` with `title`, `description`, `category`, `difficulty`.
2. Verify status 200 and JSON contains `id`.
3. GET `/api/questions` and verify the new question is in the list.

### Test Case 2: Create Question Without Title
1. POST `/api/questions` with missing `title`.
2. Verify status 400.

### Test Case 3: Create Question With Only Title
1. POST `/api/questions` with only `title`.
2. Verify status 200 and optional fields default to empty strings.

---

## B. Question Update

### Test Case 4: Update Question Text
1. Create a question.
2. PUT `/api/questions/<id>` with a new `title`.
3. Verify status 200.
4. GET `/api/questions` and verify the updated text appears.

### Test Case 5: Update Multiple Fields
1. Create a question.
2. PUT `/api/questions/<id>` with new `title`, `description`, `category`, `difficulty`.
3. Verify all changes are reflected in the list.

---

## C. Question Deletion

### Test Case 6: Delete Question
1. Create a question.
2. DELETE `/api/questions/<id>`.
3. Verify status 200.
4. GET `/api/questions` and verify the question no longer appears.

---

## D. Content Edge Cases

### Test Case 7: Special Characters in Title
1. POST `/api/questions` with title containing `@#$%^&*`.
2. Verify status 200 and text is stored literally.

### Test Case 8: Unicode in Title
1. POST `/api/questions` with title containing Unicode/emoji.
2. Verify status 200 and characters are preserved.

### Test Case 9: Long Title Text
1. POST `/api/questions` with title longer than 1000 characters.
2. Verify status 200.

### Test Case 10: SQL Injection in Title
1. POST `/api/questions` with title `"' OR 1=1 --"`.
2. Verify status 200 and text is stored literally without SQL injection.

### Test Case 11: XSS in Title
1. POST `/api/questions` with title `<script>alert('xss')</script>`.
2. Verify status 200 (or 400 if rejected) and no script execution on retrieval.
