from __future__ import annotations

import json
import logging

import nats
from nats.aio.client import Client as NATSClient
from nats.aio.msg import Msg
from nats.js import JetStreamContext

from keystone.task_claimer import TaskClaimer

logger = logging.getLogger(__name__)

# Wildcard subject to receive all task events (updated, completed, failed, etc.)
TASK_SUBJECT = "hi.tasks.>"

# Durable consumer name for JetStream at-least-once delivery
DURABLE_NAME = "keystone-dag"


class NATSListener:
    """Subscribes to NATS task events and triggers DAG advancement on completions."""

    def __init__(self, nats_url: str, task_claimer: TaskClaimer) -> None:
        self._nats_url = nats_url
        self._task_claimer = task_claimer
        self._nc: NATSClient | None = None
        self._js: JetStreamContext | None = None

    async def start(self) -> None:
        """Connect to NATS and subscribe to task events via JetStream durable consumer."""
        logger.info("Connecting to NATS at %s", self._nats_url)
        self._nc = await nats.connect(self._nats_url)
        self._js = self._nc.jetstream()
        await self._js.subscribe(TASK_SUBJECT, durable=DURABLE_NAME, cb=self._on_task_event)
        logger.info("Subscribed to %s (durable=%s)", TASK_SUBJECT, DURABLE_NAME)

    async def _on_task_event(self, msg: Msg) -> None:
        """Handle incoming task events.

        Subject format: hi.tasks.{team_id}.{task_id}.{verb}  (5 parts)
        Payload: JSON with at least {"status": "...", ...}
        When verb is "completed" or payload status is "completed", advance the DAG.
        """
        subject = msg.subject
        parts = subject.split(".")
        # Expected: hi . tasks . {team_id} . {task_id} . {verb}  (5 parts)
        if len(parts) < 5:
            logger.warning("Unexpected subject format: %s", subject)
            return

        team_id = parts[2]
        task_id = parts[3]
        verb = parts[4]

        try:
            payload = json.loads(msg.data.decode())
        except Exception as exc:
            logger.warning("Failed to parse NATS message on %s: %s", subject, exc)
            return

        status = payload.get("status") or payload.get("newStatus")
        logger.debug(
            "Task event: team=%s task=%s verb=%s status=%s",
            team_id, task_id, verb, status,
        )

        # Advance DAG if the subject verb is "completed" or the payload status is "completed"
        if verb == "completed" or status == "completed":
            logger.info(
                "Task %s in team %s completed — triggering DAG advancement",
                task_id,
                team_id,
            )
            await self._task_claimer.advance_dag(team_id)

    async def stop(self) -> None:
        """Drain and close the NATS connection."""
        if self._nc:
            await self._nc.drain()
            logger.info("NATS connection closed")
