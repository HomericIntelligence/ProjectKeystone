"""Mock-based tests for TaskClaimer."""

from __future__ import annotations

from unittest.mock import AsyncMock, MagicMock

import pytest

from keystone.maestro_client import MaestroClient
from keystone.models import Agent, Task, TaskStatus
from keystone.task_claimer import TaskClaimer


def _make_task(
    task_id: str,
    status: TaskStatus = TaskStatus.TODO,
    depends_on: list[str] | None = None,
    team_id: str = "team-1",
) -> Task:
    return Task(
        id=task_id,
        title=f"Task {task_id}",
        status=status,
        depends_on=depends_on or [],
        team_id=team_id,
    )


def _make_agent(agent_id: str, status: str = "active", task_desc: str = "") -> Agent:
    return Agent(
        id=agent_id,
        name=f"Agent {agent_id}",
        host="localhost",
        status=status,
        task_description=task_desc,
    )


def _mock_client(tasks: list[Task], agents: list[Agent]) -> MaestroClient:
    client = MagicMock(spec=MaestroClient)
    client.get_tasks = AsyncMock(return_value=tasks)
    client.get_agents = AsyncMock(return_value=agents)
    client.update_task = AsyncMock(return_value={})
    return client


# === advance_dag ===


@pytest.mark.asyncio
async def test_advance_dag_claims_ready_tasks() -> None:
    """A ready task (deps all done) gets claimed for an available agent."""
    tasks = [
        _make_task("t1", status=TaskStatus.DONE),
        _make_task("t2", depends_on=["t1"]),  # ready after t1 done
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
        assigned_to="agent-1",
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
        _make_task("t2", depends_on=["t1"]),  # t1 not done yet
    ]
    agents = [_make_agent("agent-1")]
    client = _mock_client(tasks, agents)
    claimer = TaskClaimer(client)

    claimed = await claimer.advance_dag("team-1")

    assert claimed == []
    client.update_task.assert_not_called()


@pytest.mark.asyncio
async def test_hibernated_agents_excluded() -> None:
    """Hibernated agents are not considered available."""
    tasks = [_make_task("t1")]
    agents = [_make_agent("agent-1", status="hibernated")]
    client = _mock_client(tasks, agents)
    claimer = TaskClaimer(client)

    claimed = await claimer.advance_dag("team-1")

    assert claimed == []
    client.update_task.assert_not_called()


@pytest.mark.asyncio
async def test_busy_agents_excluded() -> None:
    """Agents with a non-empty task_description are treated as occupied."""
    tasks = [_make_task("t1")]
    agents = [_make_agent("agent-1", task_desc="Working on something")]
    client = _mock_client(tasks, agents)
    claimer = TaskClaimer(client)

    claimed = await claimer.advance_dag("team-1")

    assert claimed == []
    client.update_task.assert_not_called()


# === claim_task ===


@pytest.mark.asyncio
async def test_claim_task_success() -> None:
    """claim_task returns True when update_task succeeds."""
    client = MagicMock(spec=MaestroClient)
    client.update_task = AsyncMock(return_value={})
    claimer = TaskClaimer(client)

    result = await claimer.claim_task("team-1", "t1", "agent-1")

    assert result is True


@pytest.mark.asyncio
async def test_claim_task_failure() -> None:
    """claim_task returns False when update_task raises an exception."""
    client = MagicMock(spec=MaestroClient)
    client.update_task = AsyncMock(side_effect=Exception("HTTP 500"))
    claimer = TaskClaimer(client)

    result = await claimer.claim_task("team-1", "t1", "agent-1")

    assert result is False
