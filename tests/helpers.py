"""Shared test helper functions (issue #101).

Centralises make_task / make_agent so they are not duplicated
across test_dag_walker.py and test_task_claimer.py.
"""

from __future__ import annotations

from typing import Optional

from keystone.models import Agent, Task, TaskStatus


def make_task(
    task_id: str = "task1",
    team_id: str = "team1",
    status: TaskStatus = TaskStatus.PENDING,
    assignee_agent_id: Optional[str] = None,
    dependencies: list[str] | None = None,
    title: str = "",
) -> Task:
    """Create a ``Task`` with sensible defaults for testing."""
    return Task(
        id=task_id,
        team_id=team_id,
        status=status,
        assignee_agent_id=assignee_agent_id,
        dependencies=dependencies or [],
        title=title,
    )


def make_agent(
    agent_id: str = "agent1",
    name: str = "Agent 1",
    status: str = "active",
    session_status: str = "online",
    current_task_id: Optional[str] = None,
    program: str = "",
    task_description: str = "",
) -> Agent:
    """Create an ``Agent`` with sensible defaults for testing."""
    return Agent(
        id=agent_id,
        name=name,
        status=status,
        session_status=session_status,
        current_task_id=current_task_id,
        program=program,
        task_description=task_description,
    )
