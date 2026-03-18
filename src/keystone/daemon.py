"""Main daemon entry point for ProjectKeystone.

Includes:
- NATS reconnection callbacks (issue #88)
- Periodic background DAG scan (issue #98)
- asyncio.get_running_loop() fix (issue #102)
- Health check server (issue #93)
- Structured logging (issue #96)
"""

from __future__ import annotations

import argparse
import asyncio
import signal
from typing import Any

import nats
from nats.aio.client import Client as NATSClient

from keystone.config import get_settings
from keystone.dag_walker import DAGWalker
from keystone.health import (
    mark_maestro_success,
    mark_nats_connected,
    mark_nats_disconnected,
    start_health_server,
)
from keystone.logging import configure_logging, get_logger
from keystone.maestro_client import MaestroClient
from keystone.nats_listener import NATSListener

logger = get_logger(component="daemon")

_shutdown = asyncio.Event()


def _register_signals() -> None:
    """Register signal handlers for graceful shutdown.

    Uses ``get_running_loop()`` instead of deprecated
    ``get_event_loop()`` (issue #102).
    """
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, _shutdown.set)


async def advance_dag(
    team_id: str,
    task_id: str | None = None,
    *,
    client: MaestroClient | None = None,
) -> None:
    """Fetch tasks and agents for *team_id*, then advance the DAG."""
    _client = client or MaestroClient()
    try:
        tasks = await _client.get_tasks(team_id)
        agents = await _client.get_agents()
        mark_maestro_success()

        walker = DAGWalker(tasks, agents)
        assignments = walker.advance_dag()

        for task, agent in assignments:
            await _client.update_task(
                team_id,
                task.id,
                {
                    "assigneeAgentId": agent.id,
                    "status": "in_progress",
                },
            )
            logger.info(
                "task_assignment_persisted",
                team_id=team_id,
                task_id=task.id,
                agent_id=agent.id,
            )
    except Exception:
        logger.exception("advance_dag_failed", team_id=team_id, task_id=task_id)
    finally:
        if client is None:
            await _client.close()


async def _startup_scan(client: MaestroClient | None = None) -> None:
    """Scan all teams and advance their DAGs."""
    _client = client or MaestroClient()
    try:
        teams = await _client.get_teams()
        mark_maestro_success()
        for team in teams:
            team_id = team.get("id") or team.get("_id", "")
            if team_id:
                await advance_dag(team_id, client=_client)
    except Exception:
        logger.exception("startup_scan_failed")
    finally:
        if client is None:
            await _client.close()


async def _background_scan_task() -> None:
    """Periodically re-scan all teams as a safety net (issue #98)."""
    settings = get_settings()
    interval = settings.background_scan_interval_seconds
    logger.info("background_scan_started", interval_seconds=interval)
    while not _shutdown.is_set():
        try:
            await asyncio.wait_for(
                _shutdown.wait(), timeout=interval
            )
            break  # Shutdown requested.
        except asyncio.TimeoutError:
            pass
        try:
            await _startup_scan()
        except Exception:
            logger.exception("background_scan_failed")


# ------------------------------------------------------------------ NATS callbacks (issue #88)


async def _nats_error_cb(exc: Exception) -> None:
    logger.error("nats_error", error=str(exc))


async def _nats_disconnected_cb() -> None:
    mark_nats_disconnected()
    logger.warning("nats_disconnected")


async def _nats_reconnected_cb() -> None:
    mark_nats_connected()
    logger.info("nats_reconnected")


async def _nats_closed_cb() -> None:
    mark_nats_disconnected()
    logger.info("nats_connection_closed")


# ------------------------------------------------------------------ main


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="ProjectKeystone daemon")
    parser.add_argument(
        "--log-level",
        default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Logging level",
    )
    return parser.parse_args()


async def main(args: argparse.Namespace) -> None:
    """Run the Keystone daemon."""
    import logging as stdlib_logging

    configure_logging(level=getattr(stdlib_logging, args.log_level))
    logger.info("daemon_starting")

    settings = get_settings()
    _register_signals()

    # Start health check server (issue #93).
    health_runner = await start_health_server(settings.health_port)

    # Connect to NATS with reconnection handling (issue #88).
    nc: NATSClient = await nats.connect(
        settings.nats_url,
        error_cb=_nats_error_cb,
        disconnected_cb=_nats_disconnected_cb,
        reconnected_cb=_nats_reconnected_cb,
        closed_cb=_nats_closed_cb,
        max_reconnect_attempts=settings.nats_reconnect_attempts,
        reconnect_time_wait=settings.nats_reconnect_wait_ms / 1000.0,
    )
    mark_nats_connected()
    logger.info("nats_connected", url=settings.nats_url)

    # Subscribe to task events via JetStream.
    js = nc.jetstream()
    listener = NATSListener(advance_dag_callback=advance_dag)
    await js.subscribe(
        settings.nats_subject,
        cb=listener.on_task_event,
        durable="keystone-daemon",
    )
    logger.info("nats_subscribed", subject=settings.nats_subject)

    # Run initial scan.
    await _startup_scan()

    # Start background periodic scan (issue #98).
    scan_task = asyncio.create_task(_background_scan_task())

    # Wait for shutdown signal.
    await _shutdown.wait()
    logger.info("daemon_shutting_down")

    scan_task.cancel()
    try:
        await scan_task
    except asyncio.CancelledError:
        pass

    await nc.drain()
    await health_runner.cleanup()
    logger.info("daemon_stopped")
