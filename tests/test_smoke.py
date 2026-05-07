"""Trivial import-and-init smoke test (requires SMS license)."""

import ctypes
import ctypes.util
import os


def test_phnx_imports() -> None:
    """Import phnx and access a top-level class.

    Exercises the full init path: P_SCHEMA setenv (Task 4),
    SimModSuite_licenseFile read (Task 5), SimLicense_start, and
    SMS module bring-up.
    """
    import phnx  # noqa: PLC0415

    assert phnx.Model is not None


def test_p_schema_set_after_import() -> None:
    """init.cpp must setenv P_SCHEMA from the baked PSKRNL_SCHEMA_DIR.

    Probes the C-level environment (not Python's os.environ snapshot,
    which is captured at interpreter startup before init.cpp runs).
    """
    import phnx  # noqa: F401, PLC0415  — triggers init() which calls setenv

    libc = ctypes.CDLL(ctypes.util.find_library("c") or "libc.so.6")
    libc.getenv.restype = ctypes.c_char_p
    libc.getenv.argtypes = [ctypes.c_char_p]
    raw = libc.getenv(b"P_SCHEMA")

    assert raw is not None, "P_SCHEMA was not set on import"
    schema_dir = raw.decode()
    assert os.path.isdir(schema_dir), f"P_SCHEMA={schema_dir!r} is not a directory"
