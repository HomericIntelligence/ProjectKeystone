"""ProjectKeystone daemon — automated task DAG execution for ai-maestro."""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import signal
import sys

from rich.console import Console
from rich.table import Table

from keystone.config import settings
from keystone.maestro_client import MaestroClient
from keystone.nats_listener import NATSListener
from keystone.task_claimer import TaskClaimer

console = Console()
logger = logging.getLogger(__name__)

_shutdown = asyncio.Event()


def _setup_logging(level: str) -> None:
    logging.basicConfig(
        level=level.upper(),
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%Y-%m-%dT%H:%M:%S",
    )


def _register_signals() -> None:
    loop = asyncio.get_event_loop()
    for sig in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(sig, _shutdown.set)


async def _startup_scan(task_claimer: TaskClaimer, maestro_client: MaestroClient) -> None:
    """On startup, advance any stalled DAGs across all teams."""
    logger.info("Running startup DAG scan across all teams")
    try:
        teams = await maestro_client.get_teams()
    except Exception as exc:
        logger.error("Failed to fetch teams during startup scan: %s", exc)
        return

    for team in teams:
        team_id = team.get("id")
        if not team_id:
            continue
        try:
            claimed = await task_claimer.advance_dag(str(team_id))
            if claimed:
                logger.info("Startup scan claimed %d task(s) in team %s", len(claimed), team_id)
        except Exception as exc:
            logger.error("Startup scan failed for team %s: %s", team_id, exc)


async def _print_status(maestro_client: MaestroClient) -> None:
    """Print current DAG state across all teams."""
    teams = await maestro_client.get_teams()
    for team in teams:
        team_id = str(team.get("id", ""))
        team_name = team.get("name", team_id)
        tasks = await maestro_client.get_tasks(team_id)

        table = Table(title=f"Team: {team_name} ({team_id})", show_lines=True)
        table.add_column("ID", style="dim")
        table.add_column("Title")
        table.add_column("Status", style="bold")
        table.add_column("Assigned To")
        table.add_column("Depends On")

        for task in tasks:
            table.add_row(
                task.id,
                task.title,
                task.status.value,
                task.assigned_to or "",
                ", ".join(task.depends_on),
            )
        console.print(table)


async def main(args: argparse.Namespace) -> None:
    log_level = args.log_level or settings.log_level
    _setup_logging(log_level)

    maestro_client = MaestroClient(settings.maestro_url, settings.maestro_api_key)

    if args.status:
        await _print_status(maestro_client)
        await maestro_client.close()
        return

    task_claimer = TaskClaimer(maestro_client)

    # Startup scan — advance any stalled DAGs
    await _startup_scan(task_claimer, maestro_client)

    # Start NATS listener
    listener = NATSListener(settings.nats_url, task_claimer)
    await listener.start()

    _register_signals()
    logger.info("ProjectKeystone daemon running. Waiting for task events...")

    await _shutdown.wait()

    logger.info("Shutdown signal received — stopping")
    await listener.stop()
    await maestro_client.close()
    logger.info("ProjectKeystone daemon stopped")


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="ProjectKeystone daemon")
    parser.add_argument(
        "--log-level",
        default=None,
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Override log level (default: from config/env)",
    )
    parser.add_argument(
        "--status",
        action="store_true",
        help="Print current DAG state across all teams and exit",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = _parse_args()
    try:
        asyncio.run(main(args))
    except KeyboardInterrupt:
        sys.exit(0)
