"""Configuration file for the Sphinx documentation builder."""

import os
import sys
import xml.parsers.expat  # noqa: F401 — must be imported before phnx to prevent SMS libexpat from shadowing pyexpat
from importlib.metadata import version as get_version

sys.path.append(os.path.abspath("../"))

# Allow doc builds without the compiled _core extension (CI runners don't
# have Simmetrix installed, so _core.so is never built there). Mock the
# native module if it isn't present; when the real one is available it is
# preferred so signatures and types come from the actual bindings.
try:
    import phnx._core
except ImportError:
    from unittest.mock import MagicMock

    sys.modules["phnx._core"] = MagicMock()
    sys.modules["phnx._core._core"] = MagicMock()

import phnx  # noqa: F401

project = "PHNX"
copyright = "2026, Thea Energy"  # noqa: A001
author = "Thea Energy"
release: str = get_version("phnx")

extensions = [
    "sphinx_github_style",
    "sphinx.ext.napoleon",
    "sphinx.ext.autosummary",
    "sphinx_tabs.tabs",
]
top_level = "phnx"
linkcode_blob = "head"
linkcode_url = "https://github.com/Thea-Energy/phnx"
linkcode_link_text = "Source"
add_module_names = False
templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store", "superpowers/**"]
pygments_style = "sphinx"

html_theme = "shibuya"
html_static_path = ["_static"]
html_css_files = ["custom.css"]
