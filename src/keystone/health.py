"""Minimal health check HTTP endpoint for monitoring.

Exposes ``GET /health`` returning JSON with NATS connection status,
last successful API call timestamp, and uptime. See issue #93.
"""

from __future__ import annotations

import asyncio
import time
from http import HTTPStatus
from typing import Any

from aiohttp import web

from keystone.logging import get_logger

logger = get_logger(component="health")

# Shared mutable state updated by other components.
_state: dict[str, Any] = {
    "nats_connected": False,
    "nats_last_connected": None,
    "maestro_last_success": None,
    "start_time": None,
}


def mark_nats_connected() -> None:
    """Record that NATS is currently connected."""
    _state["nats_connected"] = True
    _state["nats_last_connected"] = time.time()


def mark_nats_disconnected() -> None:
    """Record that NATS is currently disconnected."""
    _state["nats_connected"] = False


def mark_maestro_success() -> None:
    """Record a successful ai-maestro API call."""
    _state["maestro_last_success"] = time.time()


def _iso(ts: float | None) -> str | None:
    if ts is None:
        return None
    import datetime

    return datetime.datetime.fromtimestamp(ts, tz=datetime.timezone.utc).isoformat()


async def _health_handler(request: web.Request) -> web.Response:
    """Handle GET /health requests."""
    uptime = time.time() - _state["start_time"] if _state["start_time"] else 0
    body = {
        "status": "healthy" if _state["nats_connected"] else "degraded",
        "nats_connected": _state["nats_connected"],
        "nats_last_connected": _iso(_state["nats_last_connected"]),
        "maestro_last_success": _iso(_state["maestro_last_success"]),
        "uptime_seconds": round(uptime, 1),
    }
    status = HTTPStatus.OK if _state["nats_connected"] else HTTPStatus.SERVICE_UNAVAILABLE
    return web.json_response(body, status=status.value)


async def start_health_server(port: int) -> web.AppRunner:
    """Start the health check HTTP server on *port*.

    Returns the ``AppRunner`` so the caller can shut it down.
    """
    _state["start_time"] = time.time()
    app = web.Application()
    app.router.add_get("/health", _health_handler)
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, "0.0.0.0", port)
    await site.start()
    logger.info("health_server_started", port=port)
    return runner
