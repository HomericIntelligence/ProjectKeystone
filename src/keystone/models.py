from __future__ import annotations

from enum import Enum

from pydantic import BaseModel


class TaskStatus(str, Enum):
    BACKLOG = "backlog"
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    REVIEW = "review"
    COMPLETED = "completed"


class Task(BaseModel):
    id: str
    team_id: str
    subject: str
    description: str = ""
    status: TaskStatus
    assignee_agent_id: str | None = None
    blocked_by: list[str] = []  # list of task IDs (maps to API's blockedBy)
    started_at: str | None = None
    completed_at: str | None = None


class Agent(BaseModel):
    id: str
    name: str
    host: str
    status: str   # active, offline
    session_status: str = "offline"  # online, offline (from session.status)
    task_description: str = ""


class DAGState(BaseModel):
    team_id: str
    tasks: dict[str, Task]  # task_id → Task
    last_updated: str
