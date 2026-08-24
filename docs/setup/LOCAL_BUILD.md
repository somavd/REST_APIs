# Local Build Guide

Build and run the Online Compiler without Docker Compose.

## Prerequisites

- **C++17 compiler:** g++ 9+ or clang++ 10+
- **CMake:** 3.14+
- **Docker:** installed and running (for code execution)
- **Internet connection:** first build downloads Crow and Asio via CMake FetchContent

### macOS

```bash
brew install cmake
# Docker via Colima (lightweight) or Docker Desktop
brew install docker colima
colima start
```

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y g++ cmake make docker.io
sudo usermod -aG docker $USER
newgrp docker
```

## Build

```bash
cd app
mkdir -p build && cd build
cmake ..
make -j$(nproc)          # macOS: make -j$(sysctl -n hw.ncpu)
```

First build takes ~60 seconds to fetch and compile dependencies. Subsequent builds are fast.

## Run

```bash
cd app/build
./online_compiler
```

Open **http://localhost:3000** in your browser.

> The server looks for `public/` relative to the working directory. CMake copies the `public/` folder into `build/` automatically, so always run from the `build/` directory.

### With custom settings

```bash
PORT=8080 \
DOCKER_TIMEOUT_SECONDS=15 \
DOCKER_MEMORY_LIMIT=256m \
./online_compiler
```

Or use the `.env.example` file:

```bash
cp ../.env.example .env
# Edit .env as needed, then:
env $(cat .env | grep -v '^#' | xargs) ./online_compiler
```

## Verify

```bash
# Health check — should return HTML
curl -s http://localhost:3000 | head -5

# Run Python code
curl -s -X POST http://localhost:3000/api/run \
  -H 'Content-Type: application/json' \
  -d '{"language":"python","code":"print(\"hello\")"}'
# => {"stdout":"hello\n","stderr":"","exitCode":0,"timedOut":false}

# Check admin dashboard
curl -s -o /dev/null -w "%{http_code}" http://localhost:3000/public/admin-dashboard.html
# => 200
```

## Stop

Press **Ctrl+C** — the server handles SIGINT gracefully.

## Troubleshooting

| Problem | Solution |
|---------|----------|
| CMake can't find compiler | Ensure g++ or clang++ is installed |
| `make` fails with fetch errors | Check internet connection (first build needs it) |
| Docker not available warning | Start Docker: `sudo systemctl start docker` or `colima start` |
| Port already in use | Change port: `PORT=8080 ./online_compiler` |
| `public/` not found | Run from the `build/` directory, or set `PUBLIC_DIR=../public` |
