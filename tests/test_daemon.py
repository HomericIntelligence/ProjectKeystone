"""Tests for keystone.daemon.

Covers:
- advance_dag() happy path and error handling (issue #83)
- _startup_scan() with teams iteration (issue #83)
- _background_scan_task() periodic scheduling (issue #83)
- NATS reconnection callbacks (issue #88)
- _parse_args() argument parsing
"""

from __future__ import annotations

import asyncio
from unittest.mock import AsyncMock, MagicMock, patch

import pytest

from keystone.daemon import (
    _background_scan_task,
    _nats_closed_cb,
    _nats_disconnected_cb,
    _nats_error_cb,
    _nats_reconnected_cb,
    _parse_args,
    _shutdown,
    _startup_scan,
    advance_dag,
)
from tests.helpers import make_agent, make_task


# ------------------------------------------------------------------ advance_dag


class TestAdvanceDag:
    """Test the advance_dag() coroutine."""

    @pytest.mark.asyncio
    async def test_assigns_tasks_returned_by_walker(self):
        task = make_task(task_id="t1", team_id="team1")
        agent = make_agent(agent_id="a1")

        client = AsyncMock()
        client.get_tasks = AsyncMock(return_value=[task])
        client.get_agents = AsyncMock(return_value=[agent])
        client.update_task = AsyncMock()
        client.close = AsyncMock()

        with patch("keystone.daemon.DAGWalker") as MockWalker:
            MockWalker.return_value.advance_dag.return_value = [(task, agent)]
            await advance_dag("team1", client=client)

        client.get_tasks.assert_awaited_once_with("team1")
        client.get_agents.assert_awaited_once()
        client.update_task.assert_awaited_once_with(
            "team1",
            "t1",
            {"assigneeAgentId": "a1", "status": "in_progress"},
        )
        # Client passed in, so daemon should NOT close it.
        client.close.assert_not_awaited()

    @pytest.mark.asyncio
    async def test_closes_client_when_not_provided(self):
        mock_client = AsyncMock()
        mock_client.get_tasks = AsyncMock(return_value=[])
        mock_client.get_agents = AsyncMock(return_value=[])
        mock_client.close = AsyncMock()

        with patch("keystone.daemon.MaestroClient", return_value=mock_client):
            with patch("keystone.daemon.DAGWalker") as MockWalker:
                MockWalker.return_value.advance_dag.return_value = []
                await advance_dag("team1")

        mock_client.close.assert_awaited_once()

    @pytest.mark.asyncio
    async def test_handles_exception_gracefully(self):
        client = AsyncMock()
        client.get_tasks = AsyncMock(side_effect=RuntimeError("api down"))
        client.close = AsyncMock()

        # Should not raise.
        await advance_dag("team1", client=client)
        client.close.assert_not_awaited()

    @pytest.mark.asyncio
    async def test_closes_client_on_error_when_not_provided(self):
        mock_client = AsyncMock()
        mock_client.get_tasks = AsyncMock(side_effect=RuntimeError("api down"))
        mock_client.close = AsyncMock()

        with patch("keystone.daemon.MaestroClient", return_value=mock_client):
            await advance_dag("team1")

        mock_client.close.assert_awaited_once()

    @pytest.mark.asyncio
    async def test_no_assignments_means_no_updates(self):
        client = AsyncMock()
        client.get_tasks = AsyncMock(return_value=[])
        client.get_agents = AsyncMock(return_value=[])
        client.update_task = AsyncMock()
        client.close = AsyncMock()

        with patch("keystone.daemon.DAGWalker") as MockWalker:
            MockWalker.return_value.advance_dag.return_value = []
            await advance_dag("team1", client=client)

        client.update_task.assert_not_awaited()

    @pytest.mark.asyncio
    async def test_marks_maestro_success(self):
        client = AsyncMock()
        client.get_tasks = AsyncMock(return_value=[])
        client.get_agents = AsyncMock(return_value=[])
        client.close = AsyncMock()

        with patch("keystone.daemon.DAGWalker") as MockWalker:
            MockWalker.return_value.advance_dag.return_value = []
            with patch("keystone.daemon.mark_maestro_success") as mock_mark:
                await advance_dag("team1", client=client)
                mock_mark.assert_called_once()


# ------------------------------------------------------------------ _startup_scan


class TestStartupScan:
    """Test the _startup_scan() coroutine."""

    @pytest.mark.asyncio
    async def test_scans_all_teams(self):
        teams = [{"id": "team-a"}, {"id": "team-b"}]
        client = AsyncMock()
        client.get_teams = AsyncMock(return_value=teams)
        client.close = AsyncMock()

        with patch("keystone.daemon.advance_dag", new_callable=AsyncMock) as mock_adv:
            await _startup_scan(client=client)

        assert mock_adv.await_count == 2
        mock_adv.assert_any_await("team-a", client=client)
        mock_adv.assert_any_await("team-b", client=client)
        client.close.assert_not_awaited()

    @pytest.mark.asyncio
    async def test_uses_underscore_id_fallback(self):
        teams = [{"_id": "old-team"}]
        client = AsyncMock()
        client.get_teams = AsyncMock(return_value=teams)
        client.close = AsyncMock()

        with patch("keystone.daemon.advance_dag", new_callable=AsyncMock) as mock_adv:
            await _startup_scan(client=client)

        mock_adv.assert_awaited_once_with("old-team", client=client)

    @pytest.mark.asyncio
    async def test_skips_teams_without_id(self):
        teams = [{"name": "no-id-team"}]
        client = AsyncMock()
        client.get_teams = AsyncMock(return_value=teams)
        client.close = AsyncMock()

        with patch("keystone.daemon.advance_dag", new_callable=AsyncMock) as mock_adv:
            await _startup_scan(client=client)

        mock_adv.assert_not_awaited()

    @pytest.mark.asyncio
    async def test_handles_get_teams_failure(self):
        client = AsyncMock()
        client.get_teams = AsyncMock(side_effect=RuntimeError("api down"))
        client.close = AsyncMock()

        # Should not raise.
        await _startup_scan(client=client)

    @pytest.mark.asyncio
    async def test_closes_client_when_not_provided(self):
        mock_client = AsyncMock()
        mock_client.get_teams = AsyncMock(return_value=[])
        mock_client.close = AsyncMock()

        with patch("keystone.daemon.MaestroClient", return_value=mock_client):
            await _startup_scan()

        mock_client.close.assert_awaited_once()

    @pytest.mark.asyncio
    async def test_marks_maestro_success(self):
        client = AsyncMock()
        client.get_teams = AsyncMock(return_value=[])
        client.close = AsyncMock()

        with patch("keystone.daemon.mark_maestro_success") as mock_mark:
            await _startup_scan(client=client)
            mock_mark.assert_called_once()


# ------------------------------------------------------------------ _background_scan_task


class TestBackgroundScanTask:
    """Test the periodic _background_scan_task() loop."""

    @pytest.mark.asyncio
    async def test_runs_scan_then_stops_on_shutdown(self):
        """Verify the loop calls _startup_scan and exits on shutdown."""
        # Replace module-level _shutdown with a fresh Event on the current loop.
        fresh_shutdown = asyncio.Event()
        call_count = 0

        async def fake_startup_scan(**kwargs):
            nonlocal call_count
            call_count += 1
            fresh_shutdown.set()

        with patch("keystone.daemon._shutdown", fresh_shutdown):
            with patch("keystone.daemon.get_settings") as mock_settings:
                mock_settings.return_value.background_scan_interval_seconds = 0.01
                with patch("keystone.daemon._startup_scan", side_effect=fake_startup_scan):
                    await asyncio.wait_for(_background_scan_task(), timeout=2.0)

        assert call_count >= 1

    @pytest.mark.asyncio
    async def test_survives_scan_exception(self):
        """The loop should not die if _startup_scan raises."""
        fresh_shutdown = asyncio.Event()
        call_count = 0

        async def failing_scan(**kwargs):
            nonlocal call_count
            call_count += 1
            if call_count == 1:
                raise RuntimeError("scan failed")
            fresh_shutdown.set()

        with patch("keystone.daemon._shutdown", fresh_shutdown):
            with patch("keystone.daemon.get_settings") as mock_settings:
                mock_settings.return_value.background_scan_interval_seconds = 0.01
                with patch("keystone.daemon._startup_scan", side_effect=failing_scan):
                    await asyncio.wait_for(_background_scan_task(), timeout=2.0)

        assert call_count >= 2

    @pytest.mark.asyncio
    async def test_exits_immediately_on_shutdown(self):
        """If shutdown is already set, the loop exits without scanning."""
        fresh_shutdown = asyncio.Event()
        fresh_shutdown.set()

        with patch("keystone.daemon._shutdown", fresh_shutdown):
            with patch("keystone.daemon.get_settings") as mock_settings:
                mock_settings.return_value.background_scan_interval_seconds = 9999
                with patch("keystone.daemon._startup_scan", new_callable=AsyncMock) as mock_scan:
                    await asyncio.wait_for(_background_scan_task(), timeout=2.0)

        mock_scan.assert_not_awaited()


# ------------------------------------------------------------------ NATS callbacks


class TestNATSCallbacks:
    """Issue #88: NATS reconnection callbacks."""

    @pytest.mark.asyncio
    async def test_error_callback(self):
        with patch("keystone.daemon.logger") as mock_logger:
            await _nats_error_cb(RuntimeError("oops"))
            mock_logger.error.assert_called_once()

    @pytest.mark.asyncio
    async def test_disconnected_callback(self):
        with patch("keystone.daemon.mark_nats_disconnected") as mock_mark:
            await _nats_disconnected_cb()
            mock_mark.assert_called_once()

    @pytest.mark.asyncio
    async def test_reconnected_callback(self):
        with patch("keystone.daemon.mark_nats_connected") as mock_mark:
            await _nats_reconnected_cb()
            mock_mark.assert_called_once()

    @pytest.mark.asyncio
    async def test_closed_callback(self):
        with patch("keystone.daemon.mark_nats_disconnected") as mock_mark:
            await _nats_closed_cb()
            mock_mark.assert_called_once()


# ------------------------------------------------------------------ _parse_args


class TestParseArgs:
    """Test CLI argument parsing."""

    def test_defaults(self):
        with patch("sys.argv", ["daemon"]):
            args = _parse_args()
        assert args.log_level == "INFO"

    def test_custom_log_level(self):
        with patch("sys.argv", ["daemon", "--log-level", "DEBUG"]):
            args = _parse_args()
        assert args.log_level == "DEBUG"
