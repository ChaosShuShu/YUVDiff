"""Shared pytest fixtures for yuvdiff tests."""
import numpy as np
import pytest


@pytest.fixture
def rng():
    """Deterministic random generator so test failures are reproducible."""
    return np.random.default_rng(seed=20260821)
