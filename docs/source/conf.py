# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.

import os
import sys
from pathlib import Path

# sys.path.insert(0, os.path.abspath('.'))


# -- Project information -----------------------------------------------------

project = "mamba"
copyright = "2020, QuantStack & mamba contributors"  # noqa
author = "QuantStack & mamba contributors"

# The full version, including alpha/beta/rc tags
release = "0.5.2"


# -- General configuration ---------------------------------------------------

# Load local extensions (e.g. mermaid)
sys.path.insert(0, str(Path.cwd().resolve() / "tools"))

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = ["mermaid", "mermaid_inheritance", "myst_parser", "breathe"]

# Configuration of Breathe Doxygen interopt
breathe_projects = {"libmamba": os.environ.get("MAMBA_DEV_DOXYGEN_XML_DIR", "../xml")}
breathe_default_project = "libmamba"
breathe_default_members = ("members", "undoc-members")
breathe_show_include = False

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = []


# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = "sphinx_book_theme"
html_logo = "_static/logo.png"
html_title = "documentation"

html_theme_options = {
    "repository_url": "https://github.com/mamba-org/mamba",
    "use_repository_button": True,
}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ["_static"]
