"""Shared pytest fixtures for the phnx test suite."""

from pathlib import Path

import pytest

import phnx

DATA_DIR = Path(__file__).parent / "data"
PHYEOS_XT = DATA_DIR / "0012335_A.x_t"
PHYEOS_SIDECAR = DATA_DIR / "0012335_A_attrs.json"


@pytest.fixture(scope="session")
def phyeos_model():
    """Load the PHYEOS10-120 assembly with NX sidecar attributes."""
    assert PHYEOS_XT.exists(), f"Test data not found: {PHYEOS_XT}"
    assert PHYEOS_SIDECAR.exists(), f"Sidecar not found: {PHYEOS_SIDECAR}"
    return phnx.Model.from_parasolid_file(str(PHYEOS_XT), load_nx_attrs=True)


PORT_PLUG_XT = DATA_DIR / "nnnnnnn.x_t"
PORT_PLUG_SIDECAR = DATA_DIR / "0012808_attrs.json"


@pytest.fixture(scope="session")
def port_plug_model():
    """Load the NE-4 port-plug leaf .prt (Type 2 flat part) with sidecar."""
    assert PORT_PLUG_XT.exists(), f"Test data not found: {PORT_PLUG_XT}"
    assert PORT_PLUG_SIDECAR.exists(), f"Sidecar not found: {PORT_PLUG_SIDECAR}"
    return phnx.Model.from_parasolid_file(str(PORT_PLUG_XT), load_nx_attrs=True)
