from __future__ import annotations

import json
import logging

import nats
from nats.aio.client import Client as NATSClient
from nats.aio.msg import Msg

from keystone.task_claimer import TaskClaimer

logger = logging.getLogger(__name__)

# Subject pattern published by ProjectHermes for all task state changes
TASK_SUBJECT = "hi.tasks.*.*.updated"


class NATSListener:
    """Subscribes to NATS task events and triggers DAG advancement on completions."""

    def __init__(self, nats_url: str, task_claimer: TaskClaimer) -> None:
        self._nats_url = nats_url
        self._task_claimer = task_claimer
        self._nc: NATSClient | None = None

    async def start(self) -> None:
        """Connect to NATS and subscribe to task events."""
        logger.info("Connecting to NATS at %s", self._nats_url)
        self._nc = await nats.connect(self._nats_url)
        await self._nc.subscribe(TASK_SUBJECT, cb=self._on_task_event)
        logger.info("Subscribed to %s", TASK_SUBJECT)

    async def _on_task_event(self, msg: Msg) -> None:
        """Handle incoming task update events.

        Subject format: hi.tasks.{team_id}.{task_id}.updated
        Payload: JSON with at least {"status": "...", ...}
        When status transitions to "done", advance the DAG for the team.
        """
        subject = msg.subject
        parts = subject.split(".")
        # Expected: hi . tasks . {team_id} . {task_id} . updated  (6 parts)
        if len(parts) < 6:
            logger.warning("Unexpected subject format: %s", subject)
            return

        team_id = parts[2]
        task_id = parts[3]

        try:
            payload = json.loads(msg.data.decode())
        except Exception as exc:
            logger.warning("Failed to parse NATS message on %s: %s", subject, exc)
            return

        status = payload.get("status") or payload.get("newStatus")
        logger.debug(
            "Task event: team=%s task=%s status=%s", team_id, task_id, status
        )

        if status == "done":
            logger.info(
                "Task %s in team %s marked done — triggering DAG advancement",
                task_id,
                team_id,
            )
            await self._task_claimer.advance_dag(team_id)

    async def stop(self) -> None:
        """Drain and close the NATS connection."""
        if self._nc:
            await self._nc.drain()
            logger.info("NATS connection closed")
