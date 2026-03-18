"""Tests for keystone.config lazy settings (issue #103)."""

from __future__ import annotations

from keystone.config import Settings, get_settings, reset_settings, set_settings


class TestLazySettings:
    def test_get_settings_returns_singleton(self):
        reset_settings()
        s1 = get_settings()
        s2 = get_settings()
        assert s1 is s2

    def test_reset_creates_new_instance(self):
        s1 = get_settings()
        reset_settings()
        s2 = get_settings()
        assert s1 is not s2

    def test_set_settings_overrides(self):
        custom = Settings(maestro_url="http://custom:1234")
        set_settings(custom)
        assert get_settings().maestro_url == "http://custom:1234"

    def test_default_values(self):
        reset_settings()
        s = get_settings()
        assert s.nats_url == "nats://localhost:4222"
        assert s.api_max_retries == 3
        assert s.background_scan_interval_seconds == 60
