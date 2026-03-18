"""Configuration for ProjectKeystone daemon.

Uses a lazy singleton pattern so tests can override settings before
they are loaded. See issue #103.
"""

from __future__ import annotations

import os
from dataclasses import dataclass, field


@dataclass
class Settings:
    """Daemon settings loaded from environment variables."""

    nats_url: str = field(
        default_factory=lambda: os.environ.get("NATS_URL", "nats://localhost:4222")
    )
    nats_subject: str = field(
        default_factory=lambda: os.environ.get("NATS_SUBJECT", "hi.tasks.>")
    )
    nats_reconnect_attempts: int = field(
        default_factory=lambda: int(
            os.environ.get("NATS_RECONNECT_ATTEMPTS", "60")
        )
    )
    nats_reconnect_wait_ms: int = field(
        default_factory=lambda: int(
            os.environ.get("NATS_RECONNECT_WAIT_MS", "2000")
        )
    )
    maestro_url: str = field(
        default_factory=lambda: os.environ.get(
            "MAESTRO_URL", "http://172.20.0.1:23000"
        )
    )
    maestro_api_key: str = field(
        default_factory=lambda: os.environ.get("MAESTRO_API_KEY", "")
    )
    health_port: int = field(
        default_factory=lambda: int(os.environ.get("HEALTH_PORT", "8080"))
    )
    background_scan_interval_seconds: int = field(
        default_factory=lambda: int(
            os.environ.get("BACKGROUND_SCAN_INTERVAL_SECONDS", "60")
        )
    )
    api_max_retries: int = field(
        default_factory=lambda: int(os.environ.get("API_MAX_RETRIES", "3"))
    )
    api_backoff_base: float = field(
        default_factory=lambda: float(
            os.environ.get("API_BACKOFF_BASE", "1.0")
        )
    )


_settings: Settings | None = None


def get_settings() -> Settings:
    """Return the lazily-initialised settings singleton.

    Call ``reset_settings()`` in tests to force re-creation.
    """
    global _settings
    if _settings is None:
        _settings = Settings()
    return _settings


def reset_settings() -> None:
    """Reset the settings singleton (for test use)."""
    global _settings
    _settings = None


def set_settings(settings: Settings) -> None:
    """Override the settings singleton (for test use)."""
    global _settings
    _settings = settings
