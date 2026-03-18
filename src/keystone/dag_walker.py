"""DAG traversal and task scheduling logic.

Includes:
- Iterative cycle detection (issue #100, replaces recursive DFS)
- Terminal status handling for failed/error/cancelled (issue #87)
- Agent availability filtering with current_task_id check (issue #91)
"""

from __future__ import annotations

from keystone.logging import get_logger
from keystone.models import TERMINAL_STATUSES, Agent, Task, TaskStatus

logger = get_logger(component="dag_walker")


class DAGWalker:
    """Walk a team's task DAG to find ready-to-assign tasks."""

    def __init__(self, tasks: list[Task], agents: list[Agent]) -> None:
        self.tasks = {t.id: t for t in tasks}
        self.agents = agents

    def validate_no_cycles(self) -> bool:
        """Return True if the DAG has no cycles.

        Uses iterative DFS with an explicit stack to avoid
        RecursionError on deep chains (issue #100).
        """
        WHITE, GRAY, BLACK = 0, 1, 2
        color: dict[str, int] = {tid: WHITE for tid in self.tasks}

        for start_id in self.tasks:
            if color[start_id] != WHITE:
                continue
            stack: list[tuple[str, bool]] = [(start_id, False)]
            while stack:
                node_id, processed = stack.pop()
                if processed:
                    color[node_id] = BLACK
                    continue
                if color[node_id] == GRAY:
                    # Already being visited → cycle detected.
                    return False
                if color[node_id] == BLACK:
                    continue
                color[node_id] = GRAY
                stack.append((node_id, True))
                task = self.tasks[node_id]
                for dep_id in task.dependencies:
                    if dep_id not in self.tasks:
                        continue
                    if color[dep_id] == GRAY:
                        return False
                    if color[dep_id] == WHITE:
                        stack.append((dep_id, False))
        return True

    def get_ready_tasks(self) -> list[Task]:
        """Return tasks whose dependencies are all in a terminal state.

        A task is *ready* when:
        - Its own status is PENDING
        - Every dependency is in a terminal status (completed, failed, etc.)

        Tasks depending on a failed/error/cancelled dependency are still
        considered ready so that the DAG engine can decide how to handle them
        (e.g., mark them as cancelled or skip them).
        """
        ready = []
        for task in self.tasks.values():
            if task.status != TaskStatus.PENDING:
                continue
            deps_terminal = all(
                self.tasks[dep_id].status in TERMINAL_STATUSES
                for dep_id in task.dependencies
                if dep_id in self.tasks
            )
            if deps_terminal:
                ready.append(task)
        return ready

    def get_available_agents(self) -> list[Agent]:
        """Return agents that are online, active, and not currently assigned.

        Checks current_task_id to prevent double-assignment (issue #91).
        """
        return [
            a
            for a in self.agents
            if a.status == "active"
            and a.session_status == "online"
            and a.current_task_id is None
        ]

    def advance_dag(self) -> list[tuple[Task, Agent]]:
        """Match ready tasks to available agents.

        Returns a list of (task, agent) pairs for assignment.
        """
        ready = self.get_ready_tasks()
        available = self.get_available_agents()
        assignments: list[tuple[Task, Agent]] = []

        for task in ready:
            if not available:
                break
            agent = available.pop(0)
            task.assignee_agent_id = agent.id
            task.status = TaskStatus.IN_PROGRESS
            agent.current_task_id = task.id
            assignments.append((task, agent))
            logger.info(
                "task_assigned",
                task_id=task.id,
                agent_id=agent.id,
                team_id=task.team_id,
            )

        return assignments
