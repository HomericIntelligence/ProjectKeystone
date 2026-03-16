from __future__ import annotations

import logging

import httpx

from keystone.models import Agent, Task, TaskStatus

logger = logging.getLogger(__name__)


class MaestroClient:
    def __init__(self, url: str, api_key: str = "") -> None:
        self.url = url.rstrip("/")
        self.headers: dict[str, str] = {"Content-Type": "application/json"}
        if api_key:
            self.headers["Authorization"] = f"Bearer {api_key}"
        self._client: httpx.AsyncClient | None = None

    async def _http(self) -> httpx.AsyncClient:
        """Return a persistent async HTTP client, creating one if needed."""
        if self._client is None or self._client.is_closed:
            self._client = httpx.AsyncClient(
                base_url=self.url,
                headers=self.headers,
                timeout=30.0,
            )
        return self._client

    async def close(self) -> None:
        """Close the persistent HTTP client."""
        if self._client and not self._client.is_closed:
            await self._client.aclose()
            self._client = None

    async def get_tasks(self, team_id: str) -> list[Task]:
        client = await self._http()
        r = await client.get(f"/api/teams/{team_id}/tasks")
        r.raise_for_status()
        return [_task_from_api(t) for t in r.json()["tasks"]]

    async def update_task(self, team_id: str, task_id: str, **kwargs) -> tuple[Task, list[Task]]:
        """Returns (updated_task, unblocked_tasks). unblocked_tasks is ai-maestro's gift —
        it tells us which tasks just became unblocked, so we don't need to recompute."""
        # Map Python field names to API field names
        body: dict[str, object] = {}
        if "status" in kwargs:
            status = kwargs["status"]
            body["status"] = status.value if isinstance(status, TaskStatus) else status
        if "assignee_agent_id" in kwargs:
            body["assigneeAgentId"] = kwargs["assignee_agent_id"]
        if "blocked_by" in kwargs:
            body["blockedBy"] = kwargs["blocked_by"]
        client = await self._http()
        r = await client.put(
            f"/api/teams/{team_id}/tasks/{task_id}",
            json=body,
        )
        r.raise_for_status()
        data = r.json()
        return _task_from_api(data["task"]), [_task_from_api(t) for t in data.get("unblocked", [])]

    async def get_agents(self) -> list[Agent]:
        client = await self._http()
        r = await client.get("/api/agents/unified")
        r.raise_for_status()
        return [_agent_from_api(entry["agent"]) for entry in r.json()["agents"]]

    async def get_teams(self) -> list[dict]:
        client = await self._http()
        r = await client.get("/api/teams")
        r.raise_for_status()
        return r.json()["teams"]


def _task_from_api(data: dict) -> Task:
    return Task(
        id=data["id"],
        team_id=data["teamId"],
        subject=data["subject"],
        description=data.get("description", ""),
        status=TaskStatus(data["status"]),
        assignee_agent_id=data.get("assigneeAgentId"),
        blocked_by=data.get("blockedBy", []),
        started_at=data.get("startedAt"),
        completed_at=data.get("completedAt"),
    )


def _agent_from_api(data: dict) -> Agent:
    return Agent(
        id=data["id"],
        name=data["name"],
        host=data.get("hostId", "hermes"),
        status=data.get("status", "offline"),
        session_status=data.get("session", {}).get("status", "offline"),
    )
