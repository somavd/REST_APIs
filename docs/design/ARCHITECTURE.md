# Architecture

## Overview

The Online Compiler is a web-based multi-language code execution platform. Users write C++, Python, or JavaScript in the browser and execute it inside sandboxed Docker containers. An admin dashboard provides question and test case management.

## System Components

```
┌──────────────────────────────────────────────────────┐
│                     Browser                          │
│  ┌──────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │compiler  │  │admin-dashboard│  │admin-questions│  │
│  │  .html   │  │    .html     │  │    .html      │  │
│  └────┬─────┘  └──────┬───────┘  └───────┬───────┘  │
│       │               │                  │           │
└───────┼───────────────┼──────────────────┼───────────┘
        │               │                  │
        ▼               ▼                  ▼
┌──────────────────────────────────────────────────────┐
│              Crow C++ Web Server (:3000)             │
│                                                      │
│  ┌─────────┐  ┌──────────┐  ┌──────────────────┐    │
│  │  Static  │  │ /api/run │  │ /api/questions   │    │
│  │  Files   │  │          │  │ /api/testcases   │    │
│  │ /public/ │  │          │  │ /api/submissions │    │
│  └─────────┘  └────┬─────┘  └────────┬─────────┘    │
│                     │                  │              │
│                     ▼                  ▼              │
│              ┌────────────┐    ┌────────────┐        │
│              │  Executor  │    │  Database   │        │
│              │ fork/exec  │    │   SQLite    │        │
│              └─────┬──────┘    └────────────┘        │
│                    │                                  │
└────────────────────┼──────────────────────────────────┘
                     │
                     ▼
        ┌────────────────────────┐
        │   Docker Containers    │
        │  gcc:latest            │
        │  python:3-slim         │
        │  node:20-slim          │
        │  (--network none,      │
        │   --read-only,         │
        │   memory/CPU limits)   │
        └────────────────────────┘
```

## Backend (C++17 / Crow)

**Entry point:** `app/src/main.cpp`

The Crow web framework provides:
- HTTP routing (GET, POST, PUT, DELETE)
- Static file serving from `PUBLIC_DIR`
- JSON request/response handling
- Multi-threaded request handling

### Key Modules

| Module | File | Responsibility |
|--------|------|----------------|
| **Server & Routes** | `main.cpp` | HTTP routes, static serving, signal handling |
| **Executor** | `executor.cpp/hpp` | Docker container lifecycle: `fork()`, `execvp()`, `waitpid()`, `select()`-based pipe I/O, `SIGKILL` timeout |
| **Database** | `database.cpp/hpp` | SQLite CRUD for submissions, questions, test cases |
| **Config** | `config.hpp` | Environment variable parsing with defaults |
| **Validator** | `validator.hpp` | JSON request validation for `/api/run` |
| **Rate Limiter** | `rate_limiter.cpp/hpp` | Thread-safe per-IP rate limiting with stale entry cleanup |
| **Languages** | `languages.hpp` | Language config: Docker images, compile/run commands |

### Execution Flow

1. Browser sends `POST /api/run` with `{ language, code, input }`
2. `validator.hpp` validates the request
3. `rate_limiter` checks per-IP limits
4. `executor.cpp` spawns a Docker container:
   - Writes code to a temp file
   - Runs `docker run` with resource limits (`--memory`, `--cpus`, `--pids-limit`, `--network none`, `--read-only`)
   - Uses `fork/execvp` to run the Docker command
   - Reads stdout/stderr via pipes using `select()` (prevents deadlocks)
   - Enforces timeout with `SIGKILL`
   - Cleans up temp files and container
5. Result saved to SQLite via `database.cpp`
6. JSON response returned to browser

## Frontend

Static HTML/CSS/JS served by Crow from the `public/` directory.

| Page | Purpose |
|------|---------|
| `compiler.html` | Code editor with language selector, run button, output panel |
| `admin-dashboard.html` | Dashboard with question count |
| `admin-questions.html` | Question CRUD: add, edit, delete; test case management |
| `question-detail.html` | View question details and test cases |

### Shared Assets

| File | Purpose |
|------|---------|
| `compiler.css` | Compiler page styles |
| `admin.css` | Admin page styles (sidebar, cards, forms, tables) |
| `script.js` | Compiler page logic (run code, language switching) |
| `shared.js` | Common utilities (`API_BASE`, `textCell`, `showStatus`, `showError`) |

## Database (SQLite)

Single file: `data/submissions.db`

### Tables

| Table | Purpose |
|-------|---------|
| `submissions` | Code execution history (language, code, input, stdout, stderr, exit code) |
| `questions` | Coding questions (title, description, category, difficulty) |
| `test_cases` | Expected input/output per question (input, expected_output, is_hidden) |

See [`app/migrations/schema.sql`](../../app/migrations/schema.sql) for the full schema.

## Configuration

All configuration is via environment variables (see `config.hpp`):

| Variable | Default | Purpose |
|----------|---------|---------|
| `PORT` | `3000` | Server listen port |
| `PUBLIC_DIR` | `public` | Static files directory |
| `DOCKER_TIMEOUT_SECONDS` | `10` | Max execution time |
| `DOCKER_MEMORY_LIMIT` | `128m` | Container memory cap |
| `DOCKER_CPU_LIMIT` | `0.5` | CPU cores per container |
| `DOCKER_PIDS_LIMIT` | `50` | Max processes per container |
| `RATE_LIMIT_MAX_REQUESTS` | `10` | Requests per window per IP |
| `RATE_LIMIT_WINDOW_SECONDS` | `60` | Rate limit window |

## Build System

CMake 3.14+ with `FetchContent` auto-downloads:
- **Crow** web framework
- **Asio** (networking library for Crow)
- **SQLite3** (compiled as a static library)

Build output goes to `app/build/`. CMake copies `public/` into the build directory automatically.

## Docker Deployment

- **Multi-stage Dockerfile** in `app/Dockerfile`: build stage compiles the C++ binary, runtime stage copies binary + public assets
- **docker-compose.yml** in `app/`: mounts Docker socket for sibling containers, persists SQLite via named volume
- Container runs as a single process (the compiled binary)
