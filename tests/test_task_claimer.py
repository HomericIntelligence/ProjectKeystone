"""Tests for task claiming / agent assignment logic.

Uses shared fixtures from conftest.py (issue #101).
"""

from __future__ import annotations

import pytest

from keystone.dag_walker import DAGWalker
from keystone.models import TaskStatus
from tests.helpers import make_agent, make_task


class TestTaskClaiming:
    def test_single_task_single_agent(self):
        tasks = [make_task("t1")]
        agents = [make_agent("a1")]
        walker = DAGWalker(tasks, agents)
        assignments = walker.advance_dag()
        assert len(assignments) == 1
        assert assignments[0][0].status == TaskStatus.IN_PROGRESS

    def test_more_tasks_than_agents(self):
        tasks = [make_task("t1"), make_task("t2"), make_task("t3")]
        agents = [make_agent("a1")]
        walker = DAGWalker(tasks, agents)
        assignments = walker.advance_dag()
        assert len(assignments) == 1

    def test_no_ready_tasks(self):
        tasks = [make_task("t1", status=TaskStatus.IN_PROGRESS)]
        agents = [make_agent("a1")]
        walker = DAGWalker(tasks, agents)
        assert walker.advance_dag() == []

    def test_busy_agent_not_assigned(self):
        """Issue #91: agent with current_task_id skipped."""
        tasks = [make_task("t1")]
        agents = [make_agent("a1", current_task_id="other")]
        walker = DAGWalker(tasks, agents)
        assert walker.advance_dag() == []
