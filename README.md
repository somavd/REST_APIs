# Online Compiler

A high-performance web-based multi-language code compiler written in **C++17** using the [Crow](https://crowcpp.org/) web framework. Users write C++, Python, or JavaScript in the browser and execute it inside sandboxed Docker containers. Includes a submission history and an admin dashboard for managing coding questions.

This repository is split into two top-level folders:

```
OnlineCompiler/
├── app/     # Implementation — C++ backend, frontend, build & deployment files
└── docs/    # Documentation — architecture, API reference, setup guides, testing
```

## Table of Contents

- [Features](#features)
- [Tech Stack](#tech-stack)
- [Requirements](#requirements)
- [Quick Start](#quick-start)
- [Run with Docker](#run-with-docker)
- [API Reference](#api-reference)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
- [Testing](#testing)
- [Documentation](#documentation)
- [Stopping the Server](#stopping-the-server)
- [License](#license)

## Features

- C++ backend with sub-millisecond routing and native multithreading
- Multi-language code execution: C++, Python, JavaScript
- Docker sandboxing with `--network none`, `--read-only`, and memory/CPU/PID limits
- Admin authentication with cookie-based sessions
- Question CRUD with pagination, search, and filtering
- Test case CRUD (add, edit, delete)
- Question bulk delete
- Submission history
- Rate limiting — thread-safe per-IP with automatic stale cleanup
- Plain textarea editor with Ctrl+Enter to run
- Graceful shutdown on SIGINT/SIGTERM
- Input validation and 64KB output truncation
- Anonymous playground at `/playground` (no auth, no history, custom input)
- Student registration/login with cookie-based sessions
- Coding platform: published questions, test evaluation, saved submissions
- Question `published` flag to control student visibility
- Automated API test suite in `docs/testing/`

## Tech Stack

- **Backend**: C++17, Crow web framework, POSIX syscalls
- **Build**: CMake 3.14+ with FetchContent (auto-downloads Crow, Asio, SQLite)
- **Frontend**: HTML/CSS/JS, no CDN dependencies
- **Execution**: Docker containers (`gcc:14`, `python:3-alpine`, `node:20-alpine`)
- **Database**: SQLite with WAL mode

## Requirements

- C++17 compiler (g++ 9+ / clang++ 10+)
- CMake 3.14+
- Docker installed and running
- Current user in the `docker` group (no sudo needed)
- Internet connection for first build

## Quick Start

```bash
cd app
mkdir build && cd build
cmake ..
make -j$(nproc)          # macOS: make -j$(sysctl -n hw.ncpu)
./online_compiler
```

Then open <http://localhost:3000>.

> **Important:** The server looks for `public/` relative to the working directory. CMake copies `public/` into `build/` automatically, so always run from the `build/` directory.

### macOS Notes

- On macOS, use `make -j$(sysctl -n hw.ncpu)` instead of `make -j$(nproc)`.
- If using Colima for Docker, start it first: `colima start`.

### Quick Test (without browser)

```bash
# Health check
curl -s http://localhost:3000 | head -5

# Run Python code
curl -s -X POST http://localhost:3000/api/run \
  -H 'Content-Type: application/json' \
  -d '{"language":"python","code":"print(\"hello\")"}'
```

## Run with Docker

A multi-stage `Dockerfile` and `docker-compose.yml` are provided in the `app/` directory.

> **Prerequisite:** Docker must be running. On macOS with Colima, run `colima start` first.

### Docker Compose (recommended)

```bash
cd app
docker compose up --build
```

If your installation uses the older `docker-compose` (v1) binary, run this instead:

```bash
cd app
docker-compose up --build
```

Open <http://localhost:3000>. The compose file mounts the host Docker socket so the server can spawn sibling execution containers, and it persists the SQLite database in a named volume.

### Docker directly (build once, run on demand)

Build the image once, then start/stop/remove the container on demand. Rebuild only after code or config changes.

Build:

```bash
cd app
docker build -t online-compiler .
```

Run:

```bash
docker run -d \
  --name online-compiler \
  -p 3000:3000 \
  -v /var/run/docker.sock:/var/run/docker.sock \
  -v compiler-data:/app/data \
  -e PORT=3000 \
  -e DOCKER_TIMEOUT_SECONDS=10 \
  -e DOCKER_MEMORY_LIMIT=128m \
  -e DOCKER_CPU_LIMIT=0.5 \
  -e DOCKER_PIDS_LIMIT=50 \
  -e RATE_LIMIT_MAX_REQUESTS=10 \
  -e RATE_LIMIT_WINDOW_SECONDS=60 \
  online-compiler
```

Stop and start on demand:

```bash
docker stop online-compiler
docker start online-compiler
```

Remove the container:

```bash
docker stop online-compiler
docker rm online-compiler
```

Rebuild after source changes:

```bash
cd app
docker build -t online-compiler .
```

## API Reference

Base URL: `http://localhost:3000`

All endpoints return JSON with `Content-Type: application/json`.

### Authentication

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/admin/login` | Admin login; sets session cookie |
| POST | `/api/admin/logout` | Clears session cookie |
| GET  | `/api/admin/session` | Returns current admin info |

### POST /api/run

**Request:**

```json
{
  "language": "python",
  "code": "print('hello')",
  "input": "optional stdin"
}
```

**Response:**

```json
{
  "stdout": "hello\n",
  "stderr": "",
  "exitCode": 0,
  "timedOut": false
}
```

**Supported languages:** `cpp`, `python`, `javascript`

**Error responses** (400/413/429):

```json
{
  "error": "Missing or invalid 'language' field"
}
```

### Student Authentication

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/student/register` | Student registration; sets session cookie |
| POST | `/api/student/login` | Student login; sets session cookie |
| POST | `/api/student/logout` | Clears session cookie |
| GET  | `/api/student/session` | Returns current student info |

### Submissions

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/submissions?limit=&language=` | Recent execution submissions |

### Platform (requires student session)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/platform/questions` | List published questions for students |
| GET | `/api/platform/questions/<id>` | Get a published question with public test cases |
| POST | `/api/platform/submit` | Run and evaluate code against all test cases |
| GET | `/api/platform/submissions` | List logged-in student's submissions |

### Playground

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/playground/run` | Anonymous code execution (no auth, no history) |

### Questions (requires admin session)

| Method | Path | Description |
|--------|------|-------------|
| GET  | `/api/questions?page=&limit=&search=&category=&difficulty=` | List questions with pagination/search/filter |
| POST | `/api/questions` | Create a question |
| PUT  | `/api/questions/<id>` | Update a question |
| DELETE | `/api/questions/<id>` | Delete a question |
| DELETE | `/api/questions/bulk` | Bulk delete `{"ids":[...]}` |

### Test cases (requires admin session)

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/questions/<id>/testcases` | List test cases for a question |
| POST | `/api/questions/<id>/testcases` | Add a test case |
| PUT | `/api/testcases/<id>` | Update a test case |
| DELETE | `/api/testcases/<id>` | Delete a test case |

## Project Structure

```
app/
├── src/
│   ├── main.cpp             # Crow app, routes, signal handling, static serving
│   ├── auth.hpp/cpp         # Admin sessions and SHA-256 password hashing
│   ├── database.hpp/cpp     # SQLite storage: submissions, questions, admins
│   ├── executor.hpp/cpp     # Docker runner (fork/exec, select(), timeout)
│   ├── languages.hpp        # Language config (image, compile/run commands)
│   ├── config.hpp           # Env-based configuration
│   ├── validator.hpp        # JSON request validation
│   └── rate_limiter.hpp/cpp # Thread-safe rate limiter
├── public/
│   ├── compiler.html        # Code editor UI
│   ├── compiler.css
│   ├── script.js
│   ├── admin-login.html
│   ├── admin-login.js
│   ├── admin-dashboard.html
│   ├── admin-questions.html
│   ├── admin-questions.js
│   ├── question-detail.html
│   ├── question-detail.js
│   ├── admin.css
│   ├── admin.js
│   ├── shared.js
│   ├── playground.html
│   ├── playground.js
│   ├── student-login.html
│   ├── student-login.js
│   ├── student-questions.html
│   ├── student-questions.js
│   ├── student-question.html
│   ├── student-question.js
│   ├── student-submissions.html
│   └── student-submissions.js
├── migrations/
│   └── schema.sql           # SQLite schema reference
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
├── .dockerignore
└── .env.example

docs/
├── design/
│   ├── ARCHITECTURE.md      # System architecture
│   └── API_REFERENCE.md     # Full API docs
├── setup/
│   ├── LOCAL_BUILD.md       # Build without Docker
│   └── DOCKER_DEPLOY.md     # Docker deployment
└── testing/
    ├── test_helpers.py
    ├── authentication_test.py
    ├── code_execution_test.py
    ├── question_management_test.py
    ├── test_case_management_test.py
    ├── submission_history_test.py
    └── run_all_tests.sh
```

## Configuration

Set these in your shell or copy `.env.example`:

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `3000` | Server port |
| `DOCKER_TIMEOUT_SECONDS` | `10` | Max execution time per request |
| `DOCKER_MEMORY_LIMIT` | `128m` | Container memory cap |
| `DOCKER_CPU_LIMIT` | `0.5` | CPU cores per container |
| `DOCKER_PIDS_LIMIT` | `50` | Max processes per container |
| `RATE_LIMIT_MAX_REQUESTS` | `10` | Requests per window per IP |
| `RATE_LIMIT_WINDOW_SECONDS` | `60` | Rate limit window |

> Note: The C++ server reads `std::getenv()` directly. Either `export` them or run `env $(cat .env) ./online_compiler`.

## Testing

```bash
cd docs/testing
./run_all_tests.sh
```

The runner executes the Python test scripts for authentication, code execution, question management, test case management, submission history, and platform (student) flows. The server must be running and the `requests` package installed.

## Documentation

- [Architecture](docs/design/ARCHITECTURE.md)
- [API Reference](docs/design/API_REFERENCE.md)
- [Local Build Guide](docs/setup/LOCAL_BUILD.md)
- [Docker Deployment](docs/setup/DOCKER_DEPLOY.md)

## Stopping the Server

Press **Ctrl+C** — the server handles SIGINT gracefully and shuts down cleanly.

If running in Docker:

```bash
docker stop online-compiler
```

## License

ISC
