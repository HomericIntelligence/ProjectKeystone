from __future__ import annotations

from enum import Enum

from pydantic import BaseModel


class TaskStatus(str, Enum):
    TODO = "todo"
    IN_PROGRESS = "in_progress"
    REVIEW = "review"
    DONE = "done"
    BLOCKED = "blocked"


class Task(BaseModel):
    id: str
    title: str
    status: TaskStatus
    assigned_to: str | None = None
    depends_on: list[str] = []  # list of task IDs
    team_id: str


class Agent(BaseModel):
    id: str
    name: str
    host: str
    status: str  # active, hibernated, etc.
    task_description: str = ""


class DAGState(BaseModel):
    team_id: str
    tasks: dict[str, Task]  # task_id → Task
    last_updated: str
