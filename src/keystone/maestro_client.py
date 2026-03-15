from __future__ import annotations

import logging
from typing import Any

import httpx

from keystone.models import Agent, Task, TaskStatus

logger = logging.getLogger(__name__)


class MaestroClient:
    """Async HTTP client for the ai-maestro REST API."""

    def __init__(self, url: str, api_key: str) -> None:
        self._base_url = url.rstrip("/")
        self._api_key = api_key
        self._client = httpx.AsyncClient(
            base_url=self._base_url,
            headers=self._build_headers(),
            timeout=10.0,
        )

    def _build_headers(self) -> dict[str, str]:
        headers: dict[str, str] = {"Content-Type": "application/json"}
        if self._api_key:
            headers["Authorization"] = f"Bearer {self._api_key}"
        return headers

    async def get_teams(self) -> list[dict[str, Any]]:
        """GET /api/teams — list all teams."""
        response = await self._client.get("/api/teams")
        response.raise_for_status()
        return response.json()

    async def get_tasks(self, team_id: str) -> list[Task]:
        """GET /api/teams/{id}/tasks — list all tasks with dependency info."""
        response = await self._client.get(f"/api/teams/{team_id}/tasks")
        response.raise_for_status()
        raw: list[dict[str, Any]] = response.json()
        tasks: list[Task] = []
        for item in raw:
            try:
                tasks.append(
                    Task(
                        id=item["id"],
                        title=item.get("title", ""),
                        status=TaskStatus(item.get("status", "todo")),
                        assigned_to=item.get("assignedTo") or item.get("assigned_to"),
                        depends_on=item.get("dependsOn") or item.get("depends_on") or [],
                        team_id=team_id,
                    )
                )
            except Exception as exc:
                logger.warning("Skipping malformed task %s: %s", item.get("id"), exc)
        return tasks

    async def update_task(
        self,
        team_id: str,
        task_id: str,
        status: TaskStatus | None = None,
        assigned_to: str | None = None,
    ) -> dict[str, Any]:
        """PUT /api/teams/{id}/tasks/{taskId} — update task status and/or assignee."""
        payload: dict[str, Any] = {}
        if status is not None:
            payload["status"] = status.value
        if assigned_to is not None:
            payload["assignedTo"] = assigned_to
        response = await self._client.put(
            f"/api/teams/{team_id}/tasks/{task_id}", json=payload
        )
        response.raise_for_status()
        return response.json()

    async def get_agents(self) -> list[Agent]:
        """GET /api/agents/unified — list all agents across hosts."""
        response = await self._client.get("/api/agents/unified")
        response.raise_for_status()
        raw: list[dict[str, Any]] = response.json()
        agents: list[Agent] = []
        for item in raw:
            try:
                agents.append(
                    Agent(
                        id=item["id"],
                        name=item.get("name", ""),
                        host=item.get("host", ""),
                        status=item.get("status", "unknown"),
                        task_description=item.get("taskDescription")
                        or item.get("task_description")
                        or "",
                    )
                )
            except Exception as exc:
                logger.warning("Skipping malformed agent %s: %s", item.get("id"), exc)
        return agents

    async def close(self) -> None:
        await self._client.aclose()
