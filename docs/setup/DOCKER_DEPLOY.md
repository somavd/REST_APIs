# Docker Deployment Guide

Run the Online Compiler using Docker.

## Prerequisites

- **Docker Engine** with Docker Compose plugin
- Docker socket accessible to the container (for spawning execution containers)

### macOS (Colima)

```bash
brew install docker colima docker-compose
colima start --cpu 2 --memory 4
```

> After every reboot, run `colima start` to restart the Docker runtime.

### Linux

```bash
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
newgrp docker
```

---

## Docker Compose (Recommended)

### Start

```bash
cd app
docker compose up --build
```

Open **http://localhost:3000**.

### What it does

1. Builds the C++ binary inside a multi-stage Docker image
2. Mounts the host Docker socket (`/var/run/docker.sock`) so the server can spawn sibling execution containers
3. Persists the SQLite database in a named volume (`compiler-data`)
4. Sets default environment variables for resource limits
5. Includes a health check (`curl http://localhost:3000/`)

### Stop

```bash
docker compose down
```

### View logs

```bash
docker compose logs -f compiler
```

### Reset database

```bash
docker compose down -v    # removes the data volume
docker compose up --build
```

---

## Docker Run (Manual)

### Build

```bash
cd app
docker build -t online-compiler .
```

### Run

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

### Stop and remove

```bash
docker stop online-compiler
docker rm online-compiler
```

---

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `PORT` | `3000` | Server port |
| `PUBLIC_DIR` | `/app/public` | Static files directory (set in Dockerfile) |
| `DOCKER_TIMEOUT_SECONDS` | `10` | Max execution time per request |
| `DOCKER_MEMORY_LIMIT` | `128m` | Container memory cap |
| `DOCKER_CPU_LIMIT` | `0.5` | CPU cores per container |
| `DOCKER_PIDS_LIMIT` | `50` | Max processes per container |
| `RATE_LIMIT_MAX_REQUESTS` | `10` | Requests per window per IP |
| `RATE_LIMIT_WINDOW_SECONDS` | `60` | Rate limit window |

---

## Production Deployment (Linux)

### 1. Provision server

Recommended minimum: 2 vCPU, 4GB RAM, 20GB SSD.

### 2. Install Docker

```bash
sudo apt update && sudo apt upgrade -y
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER
newgrp docker
```

### 3. Clone and start

```bash
git clone <repo-url>
cd OnlineCompiler/app
docker compose up -d --build
```

### 4. Verify health

```bash
curl -sf http://localhost:3000/ && echo "Healthy" || echo "Down"
```

### 5. Auto-restart on reboot

Docker Compose already sets `restart: unless-stopped`. Ensure Docker starts on boot:

```bash
sudo systemctl enable docker
```

### 6. (Optional) Nginx reverse proxy with SSL

```bash
sudo apt install -y nginx certbot python3-certbot-nginx
```

```nginx
server {
    server_name compiler.yourdomain.com;

    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

```bash
sudo ln -s /etc/nginx/sites-available/compiler /etc/nginx/sites-enabled/
sudo nginx -t && sudo systemctl reload nginx
sudo certbot --nginx -d compiler.yourdomain.com
```

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Docker daemon not running | `sudo systemctl start docker` or `colima start` |
| Permission denied on Docker | `sudo usermod -aG docker $USER && newgrp docker` |
| Port 3000 in use | Change port mapping: `-p 8080:3000` |
| Container can't pull execution images | Ensure internet access from the host |
| Database not persisting | Check volume mount: `docker volume ls` |
