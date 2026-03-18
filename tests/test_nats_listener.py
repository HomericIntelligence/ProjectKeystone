"""Tests for keystone.nats_listener.

Covers:
- Message ACK on all code paths (issue #86)
- task.failed handling (issue #87)
- Nested payload parsing (issue #107)
- Path traversal validation (issue #113)
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import Any
from unittest.mock import AsyncMock

import pytest

from keystone.nats_listener import NATSListener, _extract_status, _validate_id


@dataclass
class FakeMsg:
    """Fake NATS message for testing."""

    subject: str = "hi.tasks.team1.task1.completed"
    data: bytes = b"{}"
    _acked: bool = field(default=False, init=False)
    _naked: bool = field(default=False, init=False)

    async def ack(self) -> None:
        self._acked = True

    async def nak(self) -> None:
        self._naked = True


def _make_msg(
    subject: str = "hi.tasks.team1.task1.completed",
    payload: dict[str, Any] | None = None,
) -> FakeMsg:
    data = json.dumps(payload).encode() if payload is not None else b"{}"
    return FakeMsg(subject=subject, data=data)


class TestMessageAck:
    """Issue #86: every code path must ACK the message."""

    @pytest.mark.asyncio
    async def test_ack_on_successful_processing(self):
        cb = AsyncMock()
        listener = NATSListener(advance_dag_callback=cb)
        msg = _make_msg()
        await listener.on_task_event(msg)
        assert msg._acked

    @pytest.mark.asyncio
    async def test_ack_on_malformed_subject(self):
        cb = AsyncMock()
        listener = NATSListener(advance_dag_callback=cb)
        msg = _make_msg(subject="bad.subject")
        await listener.on_task_event(msg)
        assert msg._acked

    @pytest.mark.asyncio
    async def test_ack_on_invalid_json(self):
        cb = AsyncMock()
        listener = NATSListener(advance_dag_callback=cb)
        msg = FakeMsg(subject="hi.tasks.team1.task1.completed", data=b"not json")
        await listener.on_task_event(msg)
        assert msg._acked

    @pytest.mark.asyncio
    async def test_ack_on_unknown_verb(self):
        cb = AsyncMock()
        listener = NATSListener(advance_dag_callback=cb)
        msg = _make_msg(subject="hi.tasks.team1.task1.unknown_verb")
        await listener.on_task_event(msg)
        assert msg._acked

    @pytest.mark.asyncio
    async def test_ack_on_callback_exception(self):
        cb = AsyncMock(side_effect=RuntimeError("boom"))
        listener = NATSListener(advance_dag_callback=cb)
        msg = _make_msg()
        await listener.on_task_event(msg)
        assert msg._acked


class TestTaskFailedHandling:
    """Issue #87: task.failed events should advance the DAG."""

    @pytest.mark.asyncio
    async def test_failed_verb_advances_dag(self):
        cb = AsyncMock()
        listener = NATSListener(advance_dag_callback=cb)
        msg = _make_msg(subject="hi.tasks.team1.task1.failed")
        await listener.on_task_event(msg)
        cb.assert_awaited_once_with(team_id="team1", task_id="task1")

    @pytest.mark.asyncio
    async def test_completed_verb_advances_dag(self):
        cb = AsyncMock()
        listener = NATSListener(advance_dag_callback=cb)
        msg = _make_msg(subject="hi.tasks.team1.task1.completed")
        await listener.on_task_event(msg)
        cb.assert_awaited_once_with(team_id="team1", task_id="task1")

    @pytest.mark.asyncio
    async def test_updated_with_failed_status_advances_dag(self):
        cb = AsyncMock()
        listener = NATSListener(advance_dag_callback=cb)
        msg = _make_msg(
            subject="hi.tasks.team1.task1.updated",
            payload={"data": {"status": "failed"}},
        )
        await listener.on_task_event(msg)
        cb.assert_awaited_once()

    @pytest.mark.asyncio
    async def test_updated_without_terminal_status_no_advance(self):
        cb = AsyncMock()
        listener = NATSListener(advance_dag_callback=cb)
        msg = _make_msg(
            subject="hi.tasks.team1.task1.updated",
            payload={"data": {"status": "in_progress"}},
        )
        await listener.on_task_event(msg)
        cb.assert_not_awaited()


class TestPayloadParsing:
    """Issue #107: status is nested inside data, not at top level."""

    def test_extract_status_from_nested_data(self):
        payload = {"event": "task.completed", "data": {"status": "completed"}}
        assert _extract_status(payload) == "completed"

    def test_extract_status_fallback_to_top_level(self):
        payload = {"status": "completed"}
        assert _extract_status(payload) == "completed"

    def test_extract_status_none_when_missing(self):
        assert _extract_status({}) is None

    def test_extract_status_prefers_nested(self):
        payload = {"status": "pending", "data": {"status": "completed"}}
        assert _extract_status(payload) == "completed"


class TestPathTraversalValidation:
    """Issue #113: NATS subject tokens must be validated."""

    def test_valid_id(self):
        assert _validate_id("team-123", "team_id") == "team-123"
        assert _validate_id("abc_def", "team_id") == "abc_def"

    def test_path_traversal_rejected(self):
        with pytest.raises(ValueError, match="Invalid team_id"):
            _validate_id("../../admin", "team_id")

    def test_slash_rejected(self):
        with pytest.raises(ValueError, match="Invalid"):
            _validate_id("team/evil", "team_id")

    def test_empty_string_rejected(self):
        with pytest.raises(ValueError, match="Invalid"):
            _validate_id("", "task_id")

    def test_dot_dot_rejected(self):
        with pytest.raises(ValueError, match="Invalid"):
            _validate_id("..", "team_id")

    @pytest.mark.asyncio
    async def test_malicious_subject_acked_not_processed(self):
        cb = AsyncMock()
        listener = NATSListener(advance_dag_callback=cb)
        msg = _make_msg(subject="hi.tasks.../../admin.task1.completed")
        await listener.on_task_event(msg)
        assert msg._acked
        cb.assert_not_awaited()
