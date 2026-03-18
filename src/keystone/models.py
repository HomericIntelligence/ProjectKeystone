"""Data models for ProjectKeystone."""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Optional


class TaskStatus(str, Enum):
    """Status of a task in the DAG.

    Includes terminal failure states (failed, error, cancelled) to match
    Hermes events and Telemachy statuses. See issues #87, #95, #108.
    """

    BACKLOG = "backlog"
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    REVIEW = "review"
    COMPLETED = "completed"
    FAILED = "failed"
    ERROR = "error"
    CANCELLED = "cancelled"


# Terminal statuses where no further progress is possible.
TERMINAL_STATUSES = frozenset(
    {
        TaskStatus.COMPLETED,
        TaskStatus.FAILED,
        TaskStatus.ERROR,
        TaskStatus.CANCELLED,
    }
)


@dataclass
class Task:
    """A single task within a team's DAG."""

    id: str
    team_id: str
    status: TaskStatus
    assignee_agent_id: Optional[str] = None
    dependencies: list[str] = field(default_factory=list)
    title: str = ""


@dataclass
class Agent:
    """An agent that can be assigned to tasks."""

    id: str
    name: str
    host: str = ""
    status: str = "active"
    session_status: str = "online"
    task_description: str = ""
    program: str = ""
    current_task_id: Optional[str] = None
