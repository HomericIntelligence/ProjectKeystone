"""Tests for keystone.health endpoint (issue #93)."""

from __future__ import annotations

import time

import pytest
from aiohttp.test_utils import AioHTTPTestCase, TestClient

from keystone.health import (
    _health_handler,
    _state,
    mark_maestro_success,
    mark_nats_connected,
    mark_nats_disconnected,
)


@pytest.fixture(autouse=True)
def _reset_health_state():
    """Reset health state before each test."""
    _state["nats_connected"] = False
    _state["nats_last_connected"] = None
    _state["maestro_last_success"] = None
    _state["start_time"] = time.time()
    yield


class TestHealthState:
    def test_mark_nats_connected(self):
        mark_nats_connected()
        assert _state["nats_connected"] is True
        assert _state["nats_last_connected"] is not None

    def test_mark_nats_disconnected(self):
        mark_nats_connected()
        mark_nats_disconnected()
        assert _state["nats_connected"] is False

    def test_mark_maestro_success(self):
        mark_maestro_success()
        assert _state["maestro_last_success"] is not None


@pytest.mark.asyncio
async def test_health_endpoint_degraded():
    """Returns 503 when NATS is disconnected."""
    from aiohttp import web
    from aiohttp.test_utils import TestServer

    app = web.Application()
    app.router.add_get("/health", _health_handler)

    async with TestClient(TestServer(app)) as client:
        resp = await client.get("/health")
        assert resp.status == 503
        body = await resp.json()
        assert body["status"] == "degraded"
        assert body["nats_connected"] is False


@pytest.mark.asyncio
async def test_health_endpoint_healthy():
    """Returns 200 when NATS is connected."""
    from aiohttp import web
    from aiohttp.test_utils import TestServer

    mark_nats_connected()

    app = web.Application()
    app.router.add_get("/health", _health_handler)

    async with TestClient(TestServer(app)) as client:
        resp = await client.get("/health")
        assert resp.status == 200
        body = await resp.json()
        assert body["status"] == "healthy"
        assert body["nats_connected"] is True
        assert body["uptime_seconds"] >= 0
