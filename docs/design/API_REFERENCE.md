# API Reference

Base URL: `http://localhost:3000`

All endpoints return JSON with `Content-Type: application/json`.

---

## Code Execution

### POST /api/run

Execute code in a sandboxed Docker container.

**Request:**
```json
{
  "language": "python",
  "code": "print('hello')",
  "input": "optional stdin"
}
```

**Supported languages:** `cpp`, `python`, `javascript`

**Success (200):**
```json
{
  "stdout": "hello\n",
  "stderr": "",
  "exitCode": 0,
  "timedOut": false
}
```

**Errors:**

| Status | Condition |
|--------|-----------|
| 400 | Missing or invalid `language` or `code` field |
| 413 | Request body exceeds 1MB |
| 429 | Rate limit exceeded |

---

## Submissions

### GET /api/submissions

Retrieve recent code execution history.

**Query parameters:**

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `limit` | int | 20 | Number of submissions to return |
| `language` | string | (all) | Filter by language |

**Success (200):**
```json
{
  "submissions": [
    {
      "id": 1,
      "language": "python",
      "code": "print('hello')",
      "input": "",
      "stdout": "hello\n",
      "stderr": "",
      "exitCode": 0,
      "timedOut": false,
      "createdAt": "2026-08-23 06:00:00"
    }
  ]
}
```

---

## Questions

### GET /api/questions

List all questions.

**Success (200):**
```json
{
  "questions": [
    {
      "id": 1,
      "title": "Two Sum",
      "description": "Given an array...",
      "category": "Arrays",
      "difficulty": "Easy",
      "createdAt": "2026-08-23 06:00:00"
    }
  ]
}
```

### POST /api/questions

Create a new question.

**Request:**
```json
{
  "title": "Two Sum",
  "description": "Given an array of integers...",
  "category": "Arrays",
  "difficulty": "Easy"
}
```

**Success (200):**
```json
{
  "success": true,
  "id": 1
}
```

**Errors:**

| Status | Condition |
|--------|-----------|
| 400 | Missing or empty `title` |

### PUT /api/questions/:id

Update an existing question.

**Request:**
```json
{
  "title": "Updated Title",
  "description": "Updated description",
  "category": "Strings",
  "difficulty": "Medium"
}
```

**Success (200):**
```json
{
  "success": true
}
```

**Errors:**

| Status | Condition |
|--------|-----------|
| 400 | Missing or empty `title` |
| 500 | Question not found or update failed |

### DELETE /api/questions/:id

Delete a question and its associated test cases.

**Success (200):**
```json
{
  "success": true
}
```

---

## Test Cases

### GET /api/questions/:questionId/testcases

List all test cases for a question.

**Success (200):**
```json
{
  "testcases": [
    {
      "id": 1,
      "question_id": 1,
      "input": "1 2 3\n5",
      "expected_output": "0 2",
      "is_hidden": false
    }
  ]
}
```

### POST /api/questions/:questionId/testcases

Add a test case to a question.

**Request:**
```json
{
  "input": "1 2 3\n5",
  "expected_output": "0 2",
  "is_hidden": false
}
```

**Success (200):**
```json
{
  "success": true,
  "id": 1
}
```

### DELETE /api/testcases/:id

Delete a test case.

**Success (200):**
```json
{
  "success": true
}
```

---

## Static Files

### GET /

Serves `compiler.html` (the code editor).

### GET /public/:path

Serves static files from the public directory. Path traversal is prevented via canonical path validation.

---

## Error Response Format

All errors follow a consistent format:

```json
{
  "error": "Description of what went wrong"
}
```

## CORS

All API responses include `Access-Control-Allow-Origin: *`.
