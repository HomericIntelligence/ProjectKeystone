"""NATS JetStream event listener for task lifecycle events.

Includes:
- Message ACK/NAK on all code paths (issue #86)
- task.failed event handling (issue #87)
- Nested payload schema parsing (issue #107)
- Path traversal validation on subject tokens (issue #113)
"""

from __future__ import annotations

import json
import re
from typing import TYPE_CHECKING, Any

from keystone.logging import get_logger
from keystone.models import TERMINAL_STATUSES, TaskStatus

if TYPE_CHECKING:
    import nats.aio.msg

logger = get_logger(component="nats_listener")

# Valid verbs extracted from NATS subjects.
_KNOWN_VERBS = frozenset(
    {"completed", "failed", "updated", "created", "assigned", "started"}
)

# Safe identifier pattern for team_id / task_id (issue #113).
_SAFE_ID = re.compile(r"^[a-zA-Z0-9_-]+$")


def _validate_id(value: str, label: str) -> str:
    """Validate that *value* is safe for use in URL path segments."""
    if not _SAFE_ID.match(value):
        raise ValueError(f"Invalid {label}: {value!r}")
    return value


def _extract_status(payload: dict[str, Any]) -> str | None:
    """Extract task status from a NATS event payload.

    Hermes nests status inside ``payload["data"]["status"]``
    (issue #107).  Fall back to top-level for backwards compatibility.
    """
    data = payload.get("data")
    if isinstance(data, dict):
        status = data.get("status")
        if status:
            return str(status)
    # Fallback to top-level (legacy format).
    return payload.get("status") or payload.get("newStatus")


class NATSListener:
    """Listens for NATS JetStream task events and drives DAG advancement."""

    def __init__(self, advance_dag_callback: Any) -> None:
        self._advance_dag = advance_dag_callback

    async def on_task_event(self, msg: nats.aio.msg.Msg) -> None:
        """Handle an incoming NATS JetStream message.

        Every code path must call ``msg.ack()`` or ``msg.nak()``
        to prevent unbounded redelivery (issue #86).
        """
        try:
            await self._process_message(msg)
            await msg.ack()
        except Exception:
            logger.exception("nats_message_processing_error", subject=msg.subject)
            # ACK even on error to prevent redelivery loop for
            # permanently unprocessable messages.
            try:
                await msg.ack()
            except Exception:
                logger.exception("nats_ack_failed", subject=msg.subject)

    async def _process_message(self, msg: nats.aio.msg.Msg) -> None:
        """Parse and process a single NATS message."""
        subject = msg.subject
        parts = subject.split(".")

        # Expected format: hi.tasks.{team_id}.{task_id}.{verb}
        if len(parts) < 5:
            logger.warning(
                "malformed_nats_subject",
                subject=subject,
                reason="expected at least 5 parts",
            )
            return

        try:
            team_id = _validate_id(parts[2], "team_id")
            task_id = _validate_id(parts[3], "task_id")
        except ValueError as exc:
            logger.warning(
                "invalid_nats_subject_token",
                subject=subject,
                error=str(exc),
            )
            return

        verb = parts[4]

        if verb not in _KNOWN_VERBS:
            logger.debug(
                "unknown_nats_verb",
                subject=subject,
                verb=verb,
            )
            return

        # Parse JSON payload.
        try:
            payload: dict[str, Any] = json.loads(msg.data) if msg.data else {}
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            logger.warning(
                "invalid_nats_payload",
                subject=subject,
                error=str(exc),
            )
            return

        # Extract status from nested data field (issue #107).
        status_str = _extract_status(payload)

        # Determine if we should advance the DAG.
        should_advance = False
        if verb in ("completed", "failed"):
            should_advance = True
        elif status_str:
            try:
                status = TaskStatus(status_str)
                if status in TERMINAL_STATUSES:
                    should_advance = True
            except ValueError:
                logger.warning(
                    "unknown_task_status",
                    subject=subject,
                    status=status_str,
                )

        if should_advance:
            logger.info(
                "advancing_dag",
                team_id=team_id,
                task_id=task_id,
                verb=verb,
                status=status_str,
            )
            await self._advance_dag(team_id=team_id, task_id=task_id)
        else:
            logger.debug(
                "nats_event_ignored",
                team_id=team_id,
                task_id=task_id,
                verb=verb,
            )
