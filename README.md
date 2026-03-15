# ProjectKeystone

Automated task DAG execution layer for [ai-maestro](https://github.com/HomericIntelligence/ai-maestro).

## Purpose

ProjectKeystone watches ai-maestro's task system and automatically advances work through the dependency graph. When a task is marked done, Keystone traverses the DAG, finds all tasks whose dependencies are now satisfied, and claims them for available agents — without any human intervention.

It does **not** replace or replicate ai-maestro's task system. It extends it with automation using only ai-maestro's existing REST API and NATS events.

## How It Works

1. **Watch**: Subscribes to NATS subject `hi.tasks.*.*.updated` (published by ProjectHermes) for all task state changes.
2. **Detect**: When a task's status transitions to `done`, Keystone identifies the affected team.
3. **Traverse**: Fetches the full task list for that team via `GET /api/teams/{id}/tasks` and walks the dependency graph.
4. **Find Ready**: Identifies tasks where every dependency is `done` and the task itself is still `todo`.
5. **Assign**: Queries `GET /api/agents/unified` for active, unoccupied agents.
6. **Claim**: Issues `PUT /api/teams/{id}/tasks/{taskId}` to set status → `in_progress` and assigns the task to the chosen agent.

## Quick Start

```bash
cp .env.example .env
# Edit .env with your MAESTRO_URL and MAESTRO_API_KEY

just start
```

## Configuration

| Variable | Default | Description |
|---|---|---|
| `MAESTRO_URL` | `http://172.20.0.1:23000` | ai-maestro base URL |
| `MAESTRO_API_KEY` | _(required)_ | API key for ai-maestro |
| `NATS_URL` | `nats://localhost:4222` | NATS server URL |
| `LOG_LEVEL` | `INFO` | Logging verbosity |

## Available Commands

```bash
just start              # Start the Keystone daemon
just dev                # Start with DEBUG logging
just status             # Show current DAG state across all teams
just advance TEAM_ID    # Manually trigger DAG advancement for a team
just test               # Run tests
just lint               # Lint with ruff
just format             # Format with ruff
```

## What Keystone Is NOT

- It does not store tasks — ai-maestro owns all task state.
- It does not have its own agent registry — it reads from ai-maestro's unified agent list.
- It does not replace ai-maestro's Kanban UI — it automates it.
