"""Shared pytest configuration for the triage tests.

* Loads the pure-Python reference model ``python/oes32_triage_ref.py`` from
  the repository checkout as ``oes32_triage_ref``, so it works without building
  the extension. ``python/`` is deliberately not put on ``sys.path``, because the
  source package ``python/oes32_triage`` would then shadow the installed one,
  which contains the compiled ``_core``.
* Registers Hypothesis profiles. ``ci`` (the default) is derandomised, so
  every run explores the same examples and CI is reproducible. ``dev`` and
  ``thorough`` explore more examples, with random seeds.
  Select a profile with ``HYPOTHESIS_PROFILE=dev``.
"""

import importlib.util
import os
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location("oes32_triage_ref", REPO / "python" / "oes32_triage_ref.py")
_ref = importlib.util.module_from_spec(_spec)
sys.modules["oes32_triage_ref"] = _ref
_spec.loader.exec_module(_ref)

try:
    from hypothesis import HealthCheck, settings

    _common = dict(deadline=None, suppress_health_check=[HealthCheck.too_slow], print_blob=True)
    settings.register_profile("ci", max_examples=200, derandomize=True, **_common)
    settings.register_profile("dev", max_examples=1000, **_common)
    settings.register_profile("thorough", max_examples=10000, **_common)
    settings.load_profile(os.environ.get("HYPOTHESIS_PROFILE", "ci"))
except ImportError:  # hypothesis is only needed by the triage test modules
    pass
