"""Configuration file for the Sphinx documentation builder."""

import os
import sys
import xml.parsers.expat  # noqa: F401 — must be imported before phnx to prevent SMS libexpat from shadowing pyexpat
from importlib.metadata import version as get_version

sys.path.append(os.path.abspath("../"))
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
