"""Configuration file for the Sphinx documentation builder."""

import os
import sys
import xml.parsers.expat  # noqa: F401 — must be imported before basalt to prevent SMS libexpat from shadowing pyexpat
from importlib.metadata import version as get_version

sys.path.append(os.path.abspath("../"))

# Always mock the compiled _core extension during docs build. Importing the
# real _core.so triggers SMS init, which requires a valid Simmetrix license
# and a reachable license file — CI runners do not have either. Docs only
# cover the public Python API.
from unittest.mock import MagicMock

sys.modules["basalt._core"] = MagicMock()
sys.modules["basalt._core._core"] = MagicMock()

import basalt  # noqa: E402, F401

project = "Basalt"
copyright = "2026, Thea Energy"  # noqa: A001
author = "Thea Energy"
release: str = get_version("basalt-mesh")

extensions = [
    "sphinx_github_style",
    "sphinx.ext.napoleon",
    "sphinx.ext.autosummary",
    "sphinx_tabs.tabs",
    "nbsphinx",
]

# Tutorial notebooks under docs/notebooks/ are pre-executed locally; outputs
# are committed in the .ipynb. RTD does not have SMS or OpenMC and must not
# re-execute. Stored exception cells fail the build (don't silently ship a
# half-broken tutorial). See docs/contributing.rst for re-execution discipline.
nbsphinx_execute = "never"
nbsphinx_allow_errors = False
top_level = "basalt"
linkcode_blob = "head"
linkcode_url = "https://github.com/Thea-Energy/basalt"
linkcode_link_text = "Source"
add_module_names = False
templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store", "superpowers/**"]
pygments_style = "sphinx"

html_theme = "shibuya"
html_static_path = ["_static"]
html_css_files = ["custom.css"]
html_logo = "_static/logo.svg"
html_favicon = "_static/logo.svg"
html_theme_options = {
    "logo_target": "/",
}
