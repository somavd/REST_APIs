# Test Case Management Testing Plan

## Pre Setup
1. Server running on `localhost:3000`.
2. Admin is logged in and a host question exists for attaching test cases.
3. `requests` Python package installed.

## Test Cases

---

## A. Test Case Creation

### Test Case 1: Add Public Test Case
1. POST `/api/questions/<question_id>/testcases` with `input`, `expected_output`, `is_hidden=false`.
2. Verify status 200 and JSON contains `id`.
3. GET `/api/questions/<question_id>/testcases` and verify the case appears.

### Test Case 2: Add Hidden Test Case
1. POST `/api/questions/<question_id>/testcases` with `is_hidden=true`.
2. Verify status 200.
3. GET test cases and verify `is_hidden=true` is preserved.

### Test Case 3: Add Test Case Without Input
1. POST `/api/questions/<question_id>/testcases` with empty `input`.
2. Verify status 200 (input defaults to empty string).

---

## B. Test Case Listing

### Test Case 4: List Test Cases for a Question
1. GET `/api/questions/<question_id>/testcases`.
2. Verify status 200 and JSON contains `testcases` array.

---

## C. Test Case Deletion

### Test Case 5: Delete Test Case
1. Add a test case.
2. DELETE `/api/testcases/<id>`.
3. Verify status 200.
4. GET test cases and verify the deleted case no longer appears.

---

## D. Content Edge Cases

### Test Case 6: Special Characters in Input/Output
1. POST a test case with `input="' OR 1=1"` and `expected_output="<b>html</b>"`.
2. Verify status 200 and values are stored literally.

### Test Case 7: Unicode in Input/Output
1. POST a test case with Unicode/emoji in input and output.
2. Verify status 200 and characters are preserved.
