# ProjectKeystone — CLAUDE.md

## Project Overview

ProjectKeystone is an automated task DAG execution layer that sits on top of ai-maestro's existing Kanban system. It watches for task completions via NATS, traverses the dependency graph, and automatically claims and assigns newly unblocked tasks to available agents.

**Critical constraint**: Keystone ONLY uses ai-maestro's REST API and NATS events. It has no database, no task storage, and no agent registry of its own.

## How DAG Traversal Works

1. A NATS message arrives on `hi.tasks.{team_id}.{task_id}.updated` with `status: "done"`.
2. `NATSListener._on_task_event` extracts `team_id` and calls `TaskClaimer.advance_dag(team_id)`.
3. `advance_dag` fetches all tasks for the team via `GET /api/teams/{team_id}/tasks`.
4. `DAGWalker.find_ready_tasks(tasks)` identifies tasks where:
   - `status == "todo"` (not already claimed or in progress)
   - All task IDs in `depends_on` resolve to tasks with `status == "done"`
5. `get_available_agents()` calls `GET /api/agents/unified` and filters for agents that are `active` and not currently assigned to a task.
6. Each ready task is paired with an available agent and claimed via `PUT /api/teams/{team_id}/tasks/{task_id}` setting `status = "in_progress"` and `assignedTo = agent_id`.

## Agent Selection Logic

- Agents are fetched from `GET /api/agents/unified` which returns agents across all hosts.
- An agent is considered available if its `status` is `"active"` and it has no current task assignment (or its current task is `done`).
- Agent-to-task matching is round-robin across available agents: tasks are zipped with agents. If there are more ready tasks than available agents, remaining tasks stay `todo` until the next DAG advancement cycle.

## Key Principles

- **No own storage**: All state lives in ai-maestro. Keystone is stateless.
- **Idempotent**: `advance_dag` can be called multiple times safely — it only claims `todo` tasks, so already-claimed tasks are skipped.
- **Event-driven**: The primary trigger is NATS. On startup, Keystone also does a full scan to advance any stalled DAGs.
- **Fail gracefully**: If ai-maestro is unreachable or returns errors, log and retry — never crash the daemon.

## Repository Structure

```
ProjectKeystone/
├── src/keystone/
│   ├── __init__.py         # version
│   ├── config.py           # settings via pydantic-settings / dotenv
│   ├── models.py           # Task, Agent, DAGState, TaskStatus
│   ├── maestro_client.py   # httpx async client for ai-maestro REST API
│   ├── dag_walker.py       # pure DAG logic (find ready, cycle detection, topo sort)
│   ├── task_claimer.py     # orchestrates fetch → find ready → claim
│   ├── nats_listener.py    # NATS subscription and event routing
│   └── daemon.py           # entrypoint, startup scan, signal handling
├── tests/
│   ├── test_dag_walker.py
│   └── test_task_claimer.py
├── justfile
├── pixi.toml
├── .env.example
└── CLAUDE.md
```

## Development Guidelines

- All Python code must use type hints throughout.
- Use `async`/`await` for all I/O (httpx async client, nats-py async API).
- Pydantic models for all data structures — never raw dicts passed between modules.
- `DAGWalker` is pure (no I/O) and fully unit-testable without mocks.
- `TaskClaimer` and `NATSListener` depend on `MaestroClient` — inject it for testing.
- Log with structured context (team_id, task_id, agent_id) at each step.

## Common Commands

```bash
just start              # Start the Keystone daemon (production)
just dev                # Start with DEBUG log level
just status             # Print current DAG state across all teams
just advance TEAM_ID    # Manually trigger advance_dag for one team
just test               # pytest
just lint               # ruff check
just format             # ruff format
```

## ai-maestro API Reference

| Method | Endpoint | Purpose |
|--------|----------|---------|
| GET | `/api/teams` | List all teams |
| GET | `/api/teams/{id}/tasks` | List tasks with dependency info |
| PUT | `/api/teams/{id}/tasks/{taskId}` | Update task status / assignedTo |
| GET | `/api/agents/unified` | List all agents across hosts |

## Task Status Values

`todo` → `in_progress` → `review` → `done` (or `blocked`)

Keystone only transitions tasks from `todo` → `in_progress`. All other transitions are performed by agents or humans in ai-maestro directly.
