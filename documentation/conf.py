# Kinetix2D documentation configuration.
#
# Build with, from the repo root:
#   sphinx-build -b html documentation documentation/_build/html

project = "Kinetix2D"
author = "djokersoft"
copyright = "2026, djokersoft"
release = "1.1.1"

extensions = []

templates_path = []
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store"]

root_doc = "index"

html_theme = "furo"
html_title = "Kinetix2D"
html_theme_options = {
    "navigation_with_keys": True,
}
