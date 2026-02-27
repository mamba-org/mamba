# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = "mamba"
copyright = "2024, QuantStack & mamba contributors"
author = "QuantStack & mamba contributors"

version = "latest"
release = "latest"

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = []

templates_path = ["_templates"]
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

language = "en"

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "alabaster"
html_static_path = ["_static"]

# The file above was generated using sphinx 8.1.3 with this command:
# sphinx-quickstart --project "mamba" --author "QuantStack & mamba contributors" -v "latest" -r "latest" -l en --no-sep --no-makefile --no-batchfile
# These are custom options for this project

html_theme = "sphinx_book_theme"
html_logo = "_static/logo.png"
html_title = "documentation"

html_theme_options = {
    "path_to_docs": "docs/source",
    "repository_branch": "main",
    "repository_url": "https://github.com/mamba-org/mamba",
    "use_download_button": True,
    "use_edit_page_button": True,
    "use_issues_button": True,
    "use_repository_button": True,
}

import os  # noqa: E402
import sys  # noqa: E402
from pathlib import Path  # noqa: E402

# Load local extensions (e.g. mermaid)
sys.path.insert(0, str(Path.cwd().resolve() / "tools"))

# Add Sphinx extension modules
extensions = [
    "mermaid",
    "mermaid_inheritance",
    "myst_parser",
    "breathe",
    "sphinx_copybutton",
]

# Configuration of Breathe Doxygen interopt
breathe_projects = {"libmamba": os.environ.get("MAMBA_DEV_DOXYGEN_XML_DIR", "../xml")}
breathe_default_project = "libmamba"
breathe_default_members = ("members", "undoc-members")
breathe_show_include = False

# Configure links which should be ignored during linkcheck
linkcheck_ignore = [
    r".*github\.com.*#",  # javascript based anchors
]
