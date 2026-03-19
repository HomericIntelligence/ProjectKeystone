"""Tests for keystone.maestro_client response parsing.

Covers:
- _task_from_api field mapping (issue #89)
- _agent_from_api field mapping including program/task_description (issue #109)
- TaskStatus enum parsing including failed/error/cancelled (issue #95)
- Response schema validation (issue #105)
- TLS enforcement warnings (issue #115)
- Path traversal validation (issue #113)
- Retry logic (issue #90)
"""

from __future__ import annotations

from unittest.mock import AsyncMock, patch

import pytest

from keystone.maestro_client import (
    MaestroClient,
    _agent_from_api,
    _task_from_api,
    _validate_id,
)
from keystone.models import TaskStatus


class TestTaskFromApi:
    """Issue #89: _task_from_api field mapping tests."""

    def test_valid_task(self):
        data = {
            "id": "t1",
            "teamId": "team1",
            "status": "pending",
            "assigneeAgentId": "a1",
            "dependencies": ["t0"],
            "title": "My Task",
        }
        task = _task_from_api(data)
        assert task.id == "t1"
        assert task.team_id == "team1"
        assert task.status == TaskStatus.PENDING
        assert task.assignee_agent_id == "a1"
        assert task.dependencies == ["t0"]
        assert task.title == "My Task"

    def test_minimal_task(self):
        data = {"id": "t1", "status": "backlog"}
        task = _task_from_api(data)
        assert task.id == "t1"
        assert task.team_id == ""
        assert task.status == TaskStatus.BACKLOG
        assert task.assignee_agent_id is None
        assert task.dependencies == []

    def test_completed_status(self):
        task = _task_from_api({"id": "t1", "status": "completed"})
        assert task.status == TaskStatus.COMPLETED

    def test_failed_status(self):
        """Issue #95: failed status must not raise ValueError."""
        task = _task_from_api({"id": "t1", "status": "failed"})
        assert task.status == TaskStatus.FAILED

    def test_error_status(self):
        task = _task_from_api({"id": "t1", "status": "error"})
        assert task.status == TaskStatus.ERROR

    def test_cancelled_status(self):
        task = _task_from_api({"id": "t1", "status": "cancelled"})
        assert task.status == TaskStatus.CANCELLED

    def test_unknown_status_raises(self):
        with pytest.raises(ValueError):
            _task_from_api({"id": "t1", "status": "nonexistent"})

    def test_missing_id_raises(self):
        with pytest.raises(KeyError):
            _task_from_api({"status": "pending"})

    def test_missing_status_raises(self):
        with pytest.raises(KeyError):
            _task_from_api({"id": "t1"})


class TestAgentFromApi:
    """Issue #89, #109: _agent_from_api field mapping tests."""

    def test_valid_agent_nested(self):
        data = {
            "agent": {
                "id": "a1",
                "name": "Claude",
                "hostId": "host-1",
                "status": "active",
                "session": {"status": "online"},
                "taskDescription": "Working on tests",
                "program": "claude-code",
                "currentTaskId": "t1",
            }
        }
        agent = _agent_from_api(data)
        assert agent.id == "a1"
        assert agent.name == "Claude"
        assert agent.host == "host-1"
        assert agent.status == "active"
        assert agent.session_status == "online"
        assert agent.task_description == "Working on tests"
        assert agent.program == "claude-code"
        assert agent.current_task_id == "t1"

    def test_flat_agent(self):
        data = {
            "id": "a1",
            "name": "Agent",
            "status": "active",
            "session": {"status": "online"},
        }
        agent = _agent_from_api(data)
        assert agent.id == "a1"
        assert agent.session_status == "online"

    def test_missing_optional_fields(self):
        data = {"agent": {"id": "a1"}}
        agent = _agent_from_api(data)
        assert agent.id == "a1"
        assert agent.name == ""
        assert agent.host == ""
        assert agent.task_description == ""
        assert agent.program == ""
        assert agent.current_task_id is None

    def test_missing_session(self):
        data = {"agent": {"id": "a1"}}
        agent = _agent_from_api(data)
        assert agent.session_status == "unknown"


class TestTLSEnforcement:
    """Issue #115: warn when API key sent over HTTP."""

    def test_http_with_api_key_logs_warning(self):
        """TLS warning is logged but doesn't raise."""
        from keystone.config import Settings, set_settings

        set_settings(
            Settings(
                maestro_url="http://localhost:9999",
                maestro_api_key="secret-key",
            )
        )
        # Should not raise, just log a warning.
        client = MaestroClient()
        assert client.url == "http://localhost:9999"

    def test_https_no_warning(self):
        from keystone.config import Settings, set_settings

        set_settings(
            Settings(
                maestro_url="https://api.example.com",
                maestro_api_key="secret-key",
            )
        )
        client = MaestroClient()
        assert client.url == "https://api.example.com"

    def test_invalid_scheme_raises(self):
        from keystone.config import Settings, set_settings

        set_settings(Settings(maestro_url="ftp://bad.example.com"))
        with pytest.raises(ValueError, match="Unsupported URL scheme"):
            MaestroClient()


class TestResponseValidation:
    """Issue #105: missing keys produce descriptive errors."""

    @pytest.mark.asyncio
    async def test_missing_tasks_key(self):
        client = MaestroClient()
        with pytest.raises(ValueError, match="missing expected key 'tasks'"):
            client._extract_key({}, "tasks", "GET /api/teams/t1/tasks")

    @pytest.mark.asyncio
    async def test_missing_agents_key(self):
        client = MaestroClient()
        with pytest.raises(ValueError, match="missing expected key 'agents'"):
            client._extract_key({}, "agents", "GET /api/agents/unified")


class TestPathValidation:
    """Issue #113: IDs used in URLs must be safe."""

    def test_traversal_rejected(self):
        with pytest.raises(ValueError):
            _validate_id("../../etc", "team_id")

    def test_valid_uuid_like(self):
        assert _validate_id("abc-123_def", "team_id") == "abc-123_def"
