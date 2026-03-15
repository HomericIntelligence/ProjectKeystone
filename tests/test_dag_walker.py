"""Tests for DAGWalker — pure DAG traversal logic."""

from __future__ import annotations

import pytest

from keystone.dag_walker import DAGWalker
from keystone.models import Task, TaskStatus


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


# === find_ready_tasks ===


def test_find_ready_tasks_no_deps() -> None:
    """A task with no dependencies and status PENDING is immediately ready."""
    tasks = {"t1": _make_task("t1")}
    ready = DAGWalker.find_ready_tasks(tasks)
    assert len(ready) == 1
    assert ready[0].id == "t1"


def test_find_ready_tasks_all_deps_done() -> None:
    """Task with all deps completed → appears in ready list."""
    tasks = {
        "t1": _make_task("t1", status=TaskStatus.COMPLETED),
        "t2": _make_task("t2", status=TaskStatus.COMPLETED),
        "t3": _make_task("t3", blocked_by=["t1", "t2"]),
    }
    ready = DAGWalker.find_ready_tasks(tasks)
    assert len(ready) == 1
    assert ready[0].id == "t3"


def test_find_ready_tasks_excludes_non_pending() -> None:
    """Tasks that are in_progress, review, completed, or backlog are not returned as ready."""
    tasks = {
        "t1": _make_task("t1", status=TaskStatus.IN_PROGRESS),
        "t2": _make_task("t2", status=TaskStatus.REVIEW),
        "t3": _make_task("t3", status=TaskStatus.COMPLETED),
        "t4": _make_task("t4", status=TaskStatus.BACKLOG),
    }
    ready = DAGWalker.find_ready_tasks(tasks)
    assert ready == []


# === blocked task ===


def test_blocked_task_incomplete_dep() -> None:
    """Task with an incomplete dependency is NOT in the ready list."""
    tasks = {
        "t1": _make_task("t1", status=TaskStatus.IN_PROGRESS),
        "t2": _make_task("t2", blocked_by=["t1"]),
    }
    ready = DAGWalker.find_ready_tasks(tasks)
    assert ready == []


def test_blocked_task_missing_dep() -> None:
    """Task whose dependency ID does not exist in the map is NOT ready (unknown dep blocks)."""
    tasks = {
        "t2": _make_task("t2", blocked_by=["t-nonexistent"]),
    }
    ready = DAGWalker.find_ready_tasks(tasks)
    assert ready == []


# === cycle detection ===


def test_cycle_detection_direct() -> None:
    """Tasks A→B→A raise ValueError."""
    tasks = {
        "A": _make_task("A", blocked_by=["B"]),
        "B": _make_task("B", blocked_by=["A"]),
    }
    with pytest.raises(ValueError, match="[Cc]ycle"):
        DAGWalker.validate_no_cycles(tasks)


def test_cycle_detection_self_loop() -> None:
    """A task depending on itself raises ValueError."""
    tasks = {
        "A": _make_task("A", blocked_by=["A"]),
    }
    with pytest.raises(ValueError, match="[Cc]ycle"):
        DAGWalker.validate_no_cycles(tasks)


def test_cycle_detection_no_cycle() -> None:
    """A valid DAG (no cycles) returns True."""
    tasks = {
        "t1": _make_task("t1"),
        "t2": _make_task("t2", blocked_by=["t1"]),
        "t3": _make_task("t3", blocked_by=["t2"]),
    }
    assert DAGWalker.validate_no_cycles(tasks) is True


def test_topological_sort_raises_on_cycle() -> None:
    """topological_sort also raises ValueError on a cyclic graph."""
    tasks = {
        "A": _make_task("A", blocked_by=["B"]),
        "B": _make_task("B", blocked_by=["A"]),
    }
    with pytest.raises(ValueError, match="[Cc]ycle"):
        DAGWalker.topological_sort(tasks)


# === topological sort ===


def test_topological_sort_linear_chain() -> None:
    """t1 → t2 → t3 should sort as [t1, t2, t3]."""
    tasks = {
        "t1": _make_task("t1"),
        "t2": _make_task("t2", blocked_by=["t1"]),
        "t3": _make_task("t3", blocked_by=["t2"]),
    }
    order = DAGWalker.topological_sort(tasks)
    assert order.index("t1") < order.index("t2") < order.index("t3")


def test_topological_sort_diamond() -> None:
    """
    t1 → t2 → t4
    t1 → t3 → t4
    t1 must be first, t4 must be last.
    """
    tasks = {
        "t1": _make_task("t1"),
        "t2": _make_task("t2", blocked_by=["t1"]),
        "t3": _make_task("t3", blocked_by=["t1"]),
        "t4": _make_task("t4", blocked_by=["t2", "t3"]),
    }
    order = DAGWalker.topological_sort(tasks)
    assert order[0] == "t1"
    assert order[-1] == "t4"
    assert order.index("t2") < order.index("t4")
    assert order.index("t3") < order.index("t4")
