"""Shared pytest fixtures (issue #101).

Test helper functions are in tests/helpers.py.
"""

from __future__ import annotations

import pytest

from keystone.config import Settings, reset_settings, set_settings


@pytest.fixture(autouse=True)
def _isolate_settings():
    """Reset the settings singleton before and after every test."""
    reset_settings()
    set_settings(
        Settings(
            nats_url="nats://localhost:4222",
            maestro_url="http://localhost:9999",
            maestro_api_key="",
            health_port=0,
            background_scan_interval_seconds=9999,
        )
    )
    yield
    reset_settings()
