"""Shared pytest fixtures for the basalt test suite."""

import json
from pathlib import Path

import pytest

import basalt

DATA_DIR = Path(__file__).parent / "data"
W7X_PATH = DATA_DIR / "scaled_w7x_stellarator.x_t"
W7X_SIDECAR_PATH = DATA_DIR / "scaled_w7x_stellarator_attrs.json"


@pytest.fixture(scope="session")
def w7x_model():
    """Load the scaled W7-X stellarator fixture with NX sidecar attributes."""
    assert W7X_PATH.exists(), f"Test data not found: {W7X_PATH}"
    assert W7X_SIDECAR_PATH.exists(), f"Sidecar not found: {W7X_SIDECAR_PATH}"
    return basalt.Model.from_parasolid_file(str(W7X_PATH), load_nx_attrs=True)


@pytest.fixture(scope="session")
def w7x_sidecar():
    """Load the W7-X sidecar JSON once per session."""
    with W7X_SIDECAR_PATH.open() as f:
        return json.load(f)
