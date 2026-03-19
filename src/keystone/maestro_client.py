"""HTTP client for the ai-maestro API.

Includes:
- Retry with exponential backoff (issue #90)
- Response schema validation (issue #105)
- TLS enforcement warnings (issue #115)
- Agent field extraction for program/task_description (issue #109)
"""

from __future__ import annotations

import asyncio
import re
from typing import Any, Awaitable, Callable
from urllib.parse import urlparse

import httpx

from keystone.config import get_settings
from keystone.logging import get_logger
from keystone.models import Agent, Task, TaskStatus

logger = get_logger(component="maestro_client")

# Matches safe identifier tokens for URL path segments (issue #113).
_SAFE_ID = re.compile(r"^[a-zA-Z0-9_-]+$")


def _validate_id(value: str, label: str) -> str:
    """Validate that *value* is safe for use in URL path segments."""
    if not _SAFE_ID.match(value):
        raise ValueError(f"Invalid {label}: {value!r}")
    return value


def _task_from_api(data: dict[str, Any]) -> Task:
    """Convert an ai-maestro API task dict to a ``Task`` model."""
    return Task(
        id=data["id"],
        team_id=data.get("teamId", ""),
        status=TaskStatus(data["status"]),
        assignee_agent_id=data.get("assigneeAgentId"),
        dependencies=data.get("dependencies", []),
        title=data.get("title", ""),
    )


def _agent_from_api(data: dict[str, Any]) -> Agent:
    """Convert an ai-maestro API agent dict to an ``Agent`` model.

    Extracts program and taskDescription fields (issue #109).
    """
    agent = data.get("agent", data)
    session = agent.get("session", {})
    return Agent(
        id=agent["id"],
        name=agent.get("name", ""),
        host=agent.get("hostId", ""),
        status=agent.get("status", "active"),
        session_status=session.get("status", "unknown"),
        task_description=agent.get("taskDescription", ""),
        program=agent.get("program", ""),
        current_task_id=agent.get("currentTaskId"),
    )


class MaestroClient:
    """Async HTTP client for ai-maestro API."""

    def __init__(self) -> None:
        settings = get_settings()
        self.url = settings.maestro_url
        self.api_key = settings.maestro_api_key
        self._max_retries = settings.api_max_retries
        self._backoff_base = settings.api_backoff_base

        self._validate_url()

        headers: dict[str, str] = {}
        if self.api_key:
            headers["Authorization"] = f"Bearer {self.api_key}"

        self._client = httpx.AsyncClient(
            base_url=self.url,
            headers=headers,
            timeout=30.0,
        )

    def _validate_url(self) -> None:
        """Validate the Maestro URL and warn about insecure configurations."""
        parsed = urlparse(self.url)
        if parsed.scheme not in ("http", "https"):
            raise ValueError(f"Unsupported URL scheme: {parsed.scheme!r}")

        logger.info(
            "maestro_client_init",
            url=self.url,
            scheme=parsed.scheme,
        )

        if parsed.scheme == "http" and self.api_key:
            logger.warning(
                "insecure_api_key_transport",
                msg="API key is being sent over plaintext HTTP. "
                "Use HTTPS in production.",
                url=self.url,
            )

    async def _with_retries(
        self, coro_factory: Callable[[], Awaitable[httpx.Response]]
    ) -> httpx.Response:
        """Execute an HTTP request with exponential backoff retry.

        *coro_factory* must be a zero-argument callable that returns
        an awaitable (so we can re-create it on each retry).
        """
        last_exc: Exception | None = None
        for attempt in range(self._max_retries):
            try:
                resp = await coro_factory()
                resp.raise_for_status()
                return resp
            except (httpx.TimeoutException, httpx.ConnectError) as exc:
                last_exc = exc
                if attempt < self._max_retries - 1:
                    wait = self._backoff_base * (2**attempt)
                    logger.warning(
                        "api_retry",
                        attempt=attempt + 1,
                        max_retries=self._max_retries,
                        wait_seconds=wait,
                        error=str(exc),
                    )
                    await asyncio.sleep(wait)
        raise last_exc  # type: ignore[misc]

    def _extract_key(
        self, body: dict[str, Any], key: str, context: str
    ) -> Any:
        """Extract a required key from an API response body.

        Raises ``ValueError`` with a descriptive message instead of
        a bare ``KeyError`` (issue #105).
        """
        if key not in body:
            raise ValueError(
                f"ai-maestro {context} response missing expected key "
                f"{key!r}; got keys: {sorted(body.keys())}"
            )
        return body[key]

    # ------------------------------------------------------------------ API

    async def get_tasks(self, team_id: str) -> list[Task]:
        """Fetch all tasks for *team_id*."""
        team_id = _validate_id(team_id, "team_id")
        resp = await self._with_retries(
            lambda: self._client.get(f"/api/teams/{team_id}/tasks")
        )
        body = resp.json()
        raw_tasks = self._extract_key(body, "tasks", f"GET /api/teams/{team_id}/tasks")
        return [_task_from_api(t) for t in raw_tasks]

    async def get_agents(self) -> list[Agent]:
        """Fetch all agents from the unified endpoint."""
        resp = await self._with_retries(
            lambda: self._client.get("/api/agents/unified")
        )
        body = resp.json()
        raw_agents = self._extract_key(body, "agents", "GET /api/agents/unified")
        return [_agent_from_api(entry) for entry in raw_agents]

    async def get_teams(self) -> list[dict[str, Any]]:
        """Fetch all teams."""
        resp = await self._with_retries(
            lambda: self._client.get("/api/teams")
        )
        body = resp.json()
        teams: list[dict[str, Any]] = self._extract_key(body, "teams", "GET /api/teams")
        return teams

    async def update_task(
        self, team_id: str, task_id: str, payload: dict[str, Any]
    ) -> Task:
        """Update a task and return the updated model."""
        team_id = _validate_id(team_id, "team_id")
        task_id = _validate_id(task_id, "task_id")
        resp = await self._with_retries(
            lambda: self._client.put(
                f"/api/teams/{team_id}/tasks/{task_id}", json=payload
            )
        )
        body = resp.json()
        raw = self._extract_key(body, "task", f"PUT /api/teams/{team_id}/tasks/{task_id}")
        return _task_from_api(raw)

    async def close(self) -> None:
        """Close the underlying HTTP client."""
        await self._client.aclose()
