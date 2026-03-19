"""Tests for keystone.dag_walker.

Covers:
- Cycle detection (issue #100 — iterative, no RecursionError)
- Ready task identification with terminal statuses (issue #87)
- Agent availability filtering with current_task_id (issue #91)
- DAG advancement logic
"""

from __future__ import annotations

import pytest

from keystone.dag_walker import DAGWalker
from keystone.models import TaskStatus
from tests.helpers import make_agent, make_task


class TestValidateNoCycles:
    def test_no_cycle_linear(self):
        tasks = [
            make_task("t1", status=TaskStatus.COMPLETED),
            make_task("t2", dependencies=["t1"]),
            make_task("t3", dependencies=["t2"]),
        ]
        walker = DAGWalker(tasks, [])
        assert walker.validate_no_cycles() is True

    def test_no_cycle_diamond(self):
        tasks = [
            make_task("t1", status=TaskStatus.COMPLETED),
            make_task("t2", dependencies=["t1"]),
            make_task("t3", dependencies=["t1"]),
            make_task("t4", dependencies=["t2", "t3"]),
        ]
        walker = DAGWalker(tasks, [])
        assert walker.validate_no_cycles() is True

    def test_cycle_detected(self):
        tasks = [
            make_task("t1", dependencies=["t3"]),
            make_task("t2", dependencies=["t1"]),
            make_task("t3", dependencies=["t2"]),
        ]
        walker = DAGWalker(tasks, [])
        assert walker.validate_no_cycles() is False

    def test_self_cycle(self):
        tasks = [make_task("t1", dependencies=["t1"])]
        walker = DAGWalker(tasks, [])
        assert walker.validate_no_cycles() is False

    def test_deep_chain_no_recursion_error(self):
        """Issue #100: 2000-deep chain must not cause RecursionError."""
        n = 2000
        tasks = [make_task(f"t{i}", status=TaskStatus.COMPLETED) for i in range(n)]
        for i in range(1, n):
            tasks[i].dependencies = [f"t{i - 1}"]
        tasks[-1].status = TaskStatus.PENDING
        walker = DAGWalker(tasks, [])
        assert walker.validate_no_cycles() is True

    def test_empty_dag(self):
        walker = DAGWalker([], [])
        assert walker.validate_no_cycles() is True


class TestGetReadyTasks:
    def test_task_with_no_deps_is_ready(self):
        tasks = [make_task("t1")]
        walker = DAGWalker(tasks, [])
        ready = walker.get_ready_tasks()
        assert [t.id for t in ready] == ["t1"]

    def test_task_with_completed_dep_is_ready(self):
        tasks = [
            make_task("t1", status=TaskStatus.COMPLETED),
            make_task("t2", dependencies=["t1"]),
        ]
        walker = DAGWalker(tasks, [])
        ready = walker.get_ready_tasks()
        assert [t.id for t in ready] == ["t2"]

    def test_task_with_pending_dep_is_not_ready(self):
        tasks = [
            make_task("t1"),
            make_task("t2", dependencies=["t1"]),
        ]
        walker = DAGWalker(tasks, [])
        ready = walker.get_ready_tasks()
        assert [t.id for t in ready] == ["t1"]

    def test_task_with_failed_dep_is_ready(self):
        """Issue #87: failed dependencies are terminal."""
        tasks = [
            make_task("t1", status=TaskStatus.FAILED),
            make_task("t2", dependencies=["t1"]),
        ]
        walker = DAGWalker(tasks, [])
        ready = walker.get_ready_tasks()
        assert [t.id for t in ready] == ["t2"]

    def test_task_with_error_dep_is_ready(self):
        tasks = [
            make_task("t1", status=TaskStatus.ERROR),
            make_task("t2", dependencies=["t1"]),
        ]
        walker = DAGWalker(tasks, [])
        ready = walker.get_ready_tasks()
        assert [t.id for t in ready] == ["t2"]

    def test_task_with_cancelled_dep_is_ready(self):
        tasks = [
            make_task("t1", status=TaskStatus.CANCELLED),
            make_task("t2", dependencies=["t1"]),
        ]
        walker = DAGWalker(tasks, [])
        ready = walker.get_ready_tasks()
        assert [t.id for t in ready] == ["t2"]

    def test_in_progress_task_not_ready(self):
        tasks = [make_task("t1", status=TaskStatus.IN_PROGRESS)]
        walker = DAGWalker(tasks, [])
        assert walker.get_ready_tasks() == []


class TestGetAvailableAgents:
    def test_online_active_agent_available(self):
        agents = [make_agent("a1")]
        walker = DAGWalker([], agents)
        assert [a.id for a in walker.get_available_agents()] == ["a1"]

    def test_offline_agent_not_available(self):
        agents = [make_agent("a1", session_status="offline")]
        walker = DAGWalker([], agents)
        assert walker.get_available_agents() == []

    def test_inactive_agent_not_available(self):
        agents = [make_agent("a1", status="inactive")]
        walker = DAGWalker([], agents)
        assert walker.get_available_agents() == []

    def test_busy_agent_not_available(self):
        """Issue #91: agent with current_task_id is not available."""
        agents = [make_agent("a1", current_task_id="some-task")]
        walker = DAGWalker([], agents)
        assert walker.get_available_agents() == []


class TestAdvanceDag:
    def test_assigns_ready_task_to_available_agent(self):
        tasks = [make_task("t1")]
        agents = [make_agent("a1")]
        walker = DAGWalker(tasks, agents)
        assignments = walker.advance_dag()
        assert len(assignments) == 1
        assert assignments[0][0].id == "t1"
        assert assignments[0][1].id == "a1"

    def test_no_agents_no_assignments(self):
        tasks = [make_task("t1")]
        walker = DAGWalker(tasks, [])
        assert walker.advance_dag() == []

    def test_multiple_assignments(self):
        tasks = [make_task("t1"), make_task("t2")]
        agents = [make_agent("a1"), make_agent("a2")]
        walker = DAGWalker(tasks, agents)
        assignments = walker.advance_dag()
        assert len(assignments) == 2
