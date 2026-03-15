from __future__ import annotations

import logging
from collections import deque

from keystone.models import Task, TaskStatus

logger = logging.getLogger(__name__)


class DAGWalker:
    """Pure DAG traversal logic — no I/O, fully unit-testable."""

    @staticmethod
    def find_ready_tasks(tasks: dict[str, Task]) -> list[Task]:
        """Return tasks where all dependencies are COMPLETED and the task itself is PENDING."""
        ready: list[Task] = []
        for task in tasks.values():
            if task.status != TaskStatus.PENDING:
                continue
            deps_done = all(
                tasks[dep_id].status == TaskStatus.COMPLETED
                for dep_id in task.blocked_by
                if dep_id in tasks
            )
            # Also treat missing dependency IDs as blocking (unknown dep)
            deps_exist = all(dep_id in tasks for dep_id in task.blocked_by)
            if deps_done and deps_exist:
                ready.append(task)
        return ready

    @staticmethod
    def find_next_for_agent(tasks: dict[str, Task], agent_id: str) -> Task | None:
        """Return the highest-priority ready task not assigned to anyone.

        Priority is determined by topological order (earlier in sort = higher priority).
        """
        try:
            order = DAGWalker.topological_sort(tasks)
        except ValueError:
            logger.error("Cycle detected in DAG — cannot determine task order")
            return None

        ready = {t.id: t for t in DAGWalker.find_ready_tasks(tasks)}
        for task_id in order:
            task = ready.get(task_id)
            if task is not None and task.assignee_agent_id is None:
                return task
        return None

    @staticmethod
    def validate_no_cycles(tasks: dict[str, Task]) -> bool:
        """Return True if the graph is acyclic. Raise ValueError if a cycle is found."""
        WHITE, GRAY, BLACK = 0, 1, 2
        color: dict[str, int] = {tid: WHITE for tid in tasks}

        def dfs(node: str) -> None:
            color[node] = GRAY
            for dep in tasks[node].blocked_by:
                if dep not in tasks:
                    continue
                if color[dep] == GRAY:
                    raise ValueError(
                        f"Cycle detected: task '{node}' depends on '{dep}' which is already being visited"
                    )
                if color[dep] == WHITE:
                    dfs(dep)
            color[node] = BLACK

        for task_id in tasks:
            if color[task_id] == WHITE:
                dfs(task_id)

        return True

    @staticmethod
    def topological_sort(tasks: dict[str, Task]) -> list[str]:
        """Return task IDs in dependency-first execution order (Kahn's algorithm).

        Raises ValueError if the graph contains a cycle.
        """
        # Build in-degree map (counting only edges to known tasks)
        in_degree: dict[str, int] = {tid: 0 for tid in tasks}
        dependents: dict[str, list[str]] = {tid: [] for tid in tasks}

        for task in tasks.values():
            for dep_id in task.blocked_by:
                if dep_id in tasks:
                    in_degree[task.id] += 1
                    dependents[dep_id].append(task.id)

        queue: deque[str] = deque(tid for tid, deg in in_degree.items() if deg == 0)
        result: list[str] = []

        while queue:
            node = queue.popleft()
            result.append(node)
            for dependent in dependents[node]:
                in_degree[dependent] -= 1
                if in_degree[dependent] == 0:
                    queue.append(dependent)

        if len(result) != len(tasks):
            raise ValueError(
                "Cycle detected in task dependency graph — topological sort failed"
            )

        return result
