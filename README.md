# Online Compiler

A web-based multi-language code compiler. Users write C++, Python, or JavaScript in the
browser and execute it inside sandboxed Docker containers. Includes a submission history
and an admin dashboard for managing coding questions.

This repository is split into two top-level folders:

```
OnlineCompiler/
├── app/     # Implementation — C++ backend, frontend, build & deployment files
└── docs/    # Documentation — project report, specs, screenshots
```

## Quick start

```bash
cd app
mkdir build && cd build
cmake ..
make -j$(nproc)          # macOS: make -j$(sysctl -n hw.ncpu)
./online_compiler
```

Then open <http://localhost:3000>.

## Run with Docker

A multi-stage `Dockerfile` and `docker-compose.yml` are provided in the `app/` directory.

### Docker Compose (recommended)

```bash
cd app
docker compose up --build
```

Open <http://localhost:3000>. The compose file mounts the host Docker socket so the server can spawn sibling execution containers, and it persists the SQLite database in a named volume.

### Docker directly

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

Stop:

```bash
docker stop online-compiler
docker rm online-compiler
```

> **Docker requirements:** Docker must be installed and running. On Linux, the current user should be in the `docker` group. On macOS with Colima, start it first with `colima start`.

- **Full build/run/API details:** see [`app/README.md`](app/README.md)
