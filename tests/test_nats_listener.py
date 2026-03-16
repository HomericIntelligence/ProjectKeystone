"""Tests for keystone.nats_listener subject parsing and event routing."""

from __future__ import annotations

import json
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from keystone.nats_listener import NATSListener


def _make_msg(subject: str, data: dict) -> MagicMock:
    """Create a fake NATS Msg with the given subject and JSON payload."""
    msg = MagicMock()
    msg.subject = subject
    msg.data = json.dumps(data).encode()
    return msg


@pytest.fixture
def listener() -> NATSListener:
    claimer = AsyncMock()
    return NATSListener(nats_url="nats://localhost:4222", task_claimer=claimer)


@pytest.mark.asyncio
async def test_completed_verb_triggers_advance(listener: NATSListener) -> None:
    """A message on hi.tasks.{team}.{task}.completed should trigger DAG advancement."""
    msg = _make_msg("hi.tasks.team1.task1.completed", {"status": "completed"})
    await listener._on_task_event(msg)
    listener._task_claimer.advance_dag.assert_awaited_once_with("team1")


@pytest.mark.asyncio
async def test_updated_with_completed_status_triggers_advance(listener: NATSListener) -> None:
    """A message on *.updated with status=completed in payload should trigger DAG advancement."""
    msg = _make_msg("hi.tasks.team2.task2.updated", {"status": "completed"})
    await listener._on_task_event(msg)
    listener._task_claimer.advance_dag.assert_awaited_once_with("team2")


@pytest.mark.asyncio
async def test_updated_with_non_completed_status_ignored(listener: NATSListener) -> None:
    """A message on *.updated with status != completed should NOT trigger DAG advancement."""
    msg = _make_msg("hi.tasks.team3.task3.updated", {"status": "in_progress"})
    await listener._on_task_event(msg)
    listener._task_claimer.advance_dag.assert_not_awaited()


@pytest.mark.asyncio
async def test_failed_verb_ignored(listener: NATSListener) -> None:
    """A message on *.failed should NOT trigger DAG advancement."""
    msg = _make_msg("hi.tasks.team4.task4.failed", {"status": "failed"})
    await listener._on_task_event(msg)
    listener._task_claimer.advance_dag.assert_not_awaited()


@pytest.mark.asyncio
async def test_malformed_subject_too_few_parts(listener: NATSListener) -> None:
    """A subject with fewer than 5 parts should be ignored gracefully."""
    msg = _make_msg("hi.tasks.team5", {"status": "completed"})
    await listener._on_task_event(msg)
    listener._task_claimer.advance_dag.assert_not_awaited()


@pytest.mark.asyncio
async def test_malformed_payload_handled(listener: NATSListener) -> None:
    """Invalid JSON payload should be logged and ignored, not crash."""
    msg = MagicMock()
    msg.subject = "hi.tasks.team6.task6.completed"
    msg.data = b"not-json"
    await listener._on_task_event(msg)
    listener._task_claimer.advance_dag.assert_not_awaited()


@pytest.mark.asyncio
async def test_five_part_subject_parsed_correctly(listener: NATSListener) -> None:
    """Verify that hi.tasks.X.Y.Z splits into exactly 5 parts and extracts team/task/verb."""
    msg = _make_msg("hi.tasks.alpha.bravo.completed", {"status": "completed"})
    await listener._on_task_event(msg)
    listener._task_claimer.advance_dag.assert_awaited_once_with("alpha")


@pytest.mark.asyncio
async def test_newstatus_field_also_checked(listener: NATSListener) -> None:
    """Payload with newStatus instead of status should also be recognized."""
    msg = _make_msg("hi.tasks.team7.task7.updated", {"newStatus": "completed"})
    await listener._on_task_event(msg)
    listener._task_claimer.advance_dag.assert_awaited_once_with("team7")
