# Authentication Testing Plan

## Pre Setup
1. Server running on `localhost:3000`.
2. Default admin exists (created on startup): `admin@example.com` / `admin123`.
3. `requests` Python package installed.

## Test Cases

---

## A. Admin Login

### Test Case 1: Valid Admin Login
1. POST `/api/admin/login` with `admin@example.com` and `admin123`.
2. Verify status 200, JSON `success=true`, and `oc_admin_session` cookie set.

### Test Case 2: Invalid Password
1. POST `/api/admin/login` with correct email and wrong password.
2. Verify status 401 and no `oc_admin_session` cookie.

### Test Case 3: Missing Password
1. POST `/api/admin/login` with only `email` field.
2. Verify status 400 and error message.

### Test Case 4: Missing Email
1. POST `/api/admin/login` with only `password` field.
2. Verify status 400.

---

## B. Session Validation

### Test Case 5: Valid Session
1. Login as admin.
2. GET `/api/admin/session` with the session cookie.
3. Verify status 200 and JSON contains `id`, `email`, `name`.

### Test Case 6: Unauthorized Session
1. GET `/api/admin/session` without any cookie.
2. Verify status 401.

---

## C. Route Protection

### Test Case 7: Protected Route Without Auth
1. GET `/api/questions` without a session cookie.
2. Verify status 401.

### Test Case 8: Protected Route With Valid Auth
1. Login as admin.
2. GET `/api/questions` with the session cookie.
3. Verify status 200.

---

## D. Admin Logout

### Test Case 9: Valid Logout
1. Login as admin.
2. POST `/api/admin/logout`.
3. Verify status 200 and `oc_admin_session` cookie cleared (Max-Age=0).

### Test Case 10: Logout Invalidates Session
1. Login as admin.
2. Logout.
3. GET `/api/admin/session` again.
4. Verify status 401.
