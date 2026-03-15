# ===================================================
# ProjectKeystone — justfile
# Automated task DAG execution for ai-maestro
# ===================================================

# === Variables ===
MAESTRO_URL := "http://172.20.0.1:23000"
NATS_URL := "nats://localhost:4222"

# === Default ===
default:
    @just --list

# === Runtime ===

# Start the Keystone daemon
start:
    MAESTRO_URL={{MAESTRO_URL}} NATS_URL={{NATS_URL}} pixi run python -m keystone.daemon

# Start with DEBUG logging
dev:
    MAESTRO_URL={{MAESTRO_URL}} NATS_URL={{NATS_URL}} LOG_LEVEL=DEBUG pixi run python -m keystone.daemon --log-level DEBUG

# Show current DAG state across all teams
status:
    MAESTRO_URL={{MAESTRO_URL}} pixi run python -m keystone.daemon --status

# Manually trigger DAG advancement for a team (for testing)
advance TEAM_ID:
    MAESTRO_URL={{MAESTRO_URL}} NATS_URL={{NATS_URL}} pixi run python -c "import asyncio; from keystone.task_claimer import TaskClaimer; from keystone.maestro_client import MaestroClient; from keystone.config import settings; c = MaestroClient(settings.maestro_url, settings.maestro_api_key); tc = TaskClaimer(c); print(asyncio.run(tc.advance_dag('{{TEAM_ID}}')))"

# === Quality ===

# Run tests
test:
    pixi run pytest tests/ -v

# Lint with ruff
lint:
    pixi run ruff check src tests

# Format with ruff
format:
    pixi run ruff format src tests
