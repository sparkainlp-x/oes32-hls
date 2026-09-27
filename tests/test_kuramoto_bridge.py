"""Tests for examples/kuramoto_oes32_bridge.py (SYNTHETIC inputs)."""

import importlib.util
import pathlib

import numpy as np
import pytest

_PATH = pathlib.Path(__file__).resolve().parents[1] / "examples" / "kuramoto_oes32_bridge.py"
_spec = importlib.util.spec_from_file_location("kuramoto_oes32_bridge", _PATH)
kb = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(kb)


def test_order_parameter_bounds():
    assert kb.kuramoto_order(np.zeros(32)) == pytest.approx(1.0)
    assert kb.kuramoto_order(np.linspace(0, 2 * np.pi, 32, endpoint=False)) == pytest.approx(0.0, abs=1e-12)


def test_phase_entropy_extremes():
    assert kb.phase_entropy(np.full(32, 0.1)) == pytest.approx(0.0)
    even = (np.arange(32) + 0.5) * (2 * np.pi / 32)
    assert kb.phase_entropy(even, bins=16) == pytest.approx(1.0)


def test_coherence_score_not_constant_zero_issue_5():
    """Regression for issue #5: the old score was 0 for every phase configuration."""
    assert kb.coherence_score(np.full(32, 1.0)) == pytest.approx(1.0)
    even = (np.arange(32) + 0.5) * (2 * np.pi / 32)
    assert kb.coherence_score(even) == pytest.approx(0.0, abs=1e-12)
    theta_hist, _ = kb.simulate_kuramoto(n=32, steps=1000, coupling=1.0, seed=42)
    first, last = kb.coherence_score(theta_hist[0]), kb.coherence_score(theta_hist[-1])
    assert 0.0 <= first <= 1.0 and 0.0 <= last <= 1.0
    assert last > first


def test_legacy_probabilities_argument_is_accepted():
    theta = np.full(32, 0.3)
    _, probs = kb.phases_to_oes32(theta)
    assert kb.coherence_score(theta, probs) == kb.coherence_score(theta)


def test_phase_entropy_rejects_bad_bins():
    with pytest.raises(ValueError):
        kb.phase_entropy(np.zeros(4), bins=1)
