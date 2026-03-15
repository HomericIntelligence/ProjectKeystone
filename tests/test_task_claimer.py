"""Mock-based tests for TaskClaimer."""

from __future__ import annotations

from unittest.mock import AsyncMock, MagicMock

import pytest

from keystone.maestro_client import MaestroClient
from keystone.models import Agent, Task, TaskStatus
from keystone.task_claimer import TaskClaimer


def _make_task(
    task_id: str,
    status: TaskStatus = TaskStatus.PENDING,
    blocked_by: list[str] | None = None,
    team_id: str = "team-1",
) -> Task:
    return Task(
        id=task_id,
        subject=f"Task {task_id}",
        status=status,
        blocked_by=blocked_by or [],
        team_id=team_id,
    )


def _make_agent(agent_id: str, status: str = "active", session_status: str = "online") -> Agent:
    return Agent(
        id=agent_id,
        name=f"Agent {agent_id}",
        host="localhost",
        status=status,
        session_status=session_status,
    )


def _mock_client(tasks: list[Task], agents: list[Agent]) -> MaestroClient:
    client = MagicMock(spec=MaestroClient)
    client.get_tasks = AsyncMock(return_value=tasks)
    client.get_agents = AsyncMock(return_value=agents)
    # update_task now returns (task, unblocked_list)
    dummy_task = tasks[0] if tasks else _make_task("dummy")
    client.update_task = AsyncMock(return_value=(dummy_task, []))
    return client


# === advance_dag ===


@pytest.mark.asyncio
async def test_advance_dag_claims_ready_tasks() -> None:
    """A ready task (deps all completed) gets claimed for an available agent."""
    tasks = [
        _make_task("t1", status=TaskStatus.COMPLETED),
        _make_task("t2", blocked_by=["t1"]),  # ready after t1 completed
    ]
    agents = [_make_agent("agent-1")]
    client = _mock_client(tasks, agents)
    claimer = TaskClaimer(client)

    claimed = await claimer.advance_dag("team-1")

    assert claimed == ["t2"]
    client.update_task.assert_called_once_with(
        team_id="team-1",
        task_id="t2",
        status=TaskStatus.IN_PROGRESS,
        assignee_agent_id="agent-1",
    )


@pytest.mark.asyncio
async def test_advance_dag_multiple_ready_tasks() -> None:
    """Multiple ready tasks are each assigned to a separate available agent."""
    tasks = [
        _make_task("t1"),
        _make_task("t2"),
        _make_task("t3"),
    ]
    agents = [_make_agent("agent-1"), _make_agent("agent-2"), _make_agent("agent-3")]
    client = _mock_client(tasks, agents)
    claimer = TaskClaimer(client)

    claimed = await claimer.advance_dag("team-1")

    assert set(claimed) == {"t1", "t2", "t3"}
    assert client.update_task.call_count == 3


@pytest.mark.asyncio
async def test_no_available_agents() -> None:
    """No available agents → tasks remain unclaimed."""
    tasks = [_make_task("t1")]
    agents: list[Agent] = []
    client = _mock_client(tasks, agents)
    claimer = TaskClaimer(client)

    claimed = await claimer.advance_dag("team-1")

    assert claimed == []
    client.update_task.assert_not_called()


@pytest.mark.asyncio
async def test_no_ready_tasks() -> None:
    """All tasks have incomplete deps → nothing is claimed."""
    tasks = [
        _make_task("t1", status=TaskStatus.IN_PROGRESS),
        _make_task("t2", blocked_by=["t1"]),  # t1 not completed yet
    ]
    agents = [_make_agent("agent-1")]
    client = _mock_client(tasks, agents)
    claimer = TaskClaimer(client)

    claimed = await claimer.advance_dag("team-1")

    assert claimed == []
    client.update_task.assert_not_called()


@pytest.mark.asyncio
async def test_offline_agents_excluded() -> None:
    """Agents with status!=active or session_status!=online are not considered available."""
    tasks = [_make_task("t1")]
    agents = [_make_agent("agent-1", status="offline")]
    client = _mock_client(tasks, agents)
    claimer = TaskClaimer(client)

    claimed = await claimer.advance_dag("team-1")

    assert claimed == []
    client.update_task.assert_not_called()


@pytest.mark.asyncio
async def test_active_but_session_offline_excluded() -> None:
    """An agent that is active but session is offline is not available."""
    tasks = [_make_task("t1")]
    agents = [_make_agent("agent-1", status="active", session_status="offline")]
    client = _mock_client(tasks, agents)
    claimer = TaskClaimer(client)

    claimed = await claimer.advance_dag("team-1")

    assert claimed == []
    client.update_task.assert_not_called()


# === claim_task ===


@pytest.mark.asyncio
async def test_claim_task_success() -> None:
    """claim_task returns (True, unblocked) when update_task succeeds."""
    dummy_task = _make_task("t1")
    client = MagicMock(spec=MaestroClient)
    client.update_task = AsyncMock(return_value=(dummy_task, []))
    claimer = TaskClaimer(client)

    success, unblocked = await claimer.claim_task("team-1", "t1", "agent-1")

    assert success is True
    assert unblocked == []


@pytest.mark.asyncio
async def test_claim_task_failure() -> None:
    """claim_task returns (False, []) when update_task raises an exception."""
    client = MagicMock(spec=MaestroClient)
    client.update_task = AsyncMock(side_effect=Exception("HTTP 500"))
    claimer = TaskClaimer(client)

    success, unblocked = await claimer.claim_task("team-1", "t1", "agent-1")

    assert success is False
    assert unblocked == []
