#!/usr/bin/env python3

# This script generates a static documentation web site
# by parsing the `**/*.asiidoc` files from the repository.
#
# Dependencies:
# * Relatively recent Python 3 - must support `typing.Optional`.
# * `xdg-open` for opening the final result in a web browser.
# * `antora` - a static documentation web site generator,
#     see: https://antora.org
#
# Usage:
# ```console
# $ ./contrib/gendocs.py
# ```
#
# It should open the generated web site in a browser.


import atexit
import glob
import linksfixer
import os
import pathlib
import shutil
import subprocess

# Get the script directory
script_dir = os.path.dirname(os.path.realpath(__file__))

# Switch to the projects root dir
os.chdir(os.path.join(script_dir, ".."))


# Idempotency
def cleanup():
    try:
        shutil.move("test/tests.asciidoc", "test/README.asciidoc")
    except BaseException:
        pass
    try:
        shutil.move("index.asciidoc", "README.asciidoc")
    except BaseException:
        pass
    try:
        shutil.move("VIMTOKAK.asciidoc", "VIMTOKAK")
    except BaseException:
        pass


# Set up cleanup to be executed on script exit
atexit.register(cleanup)

# Antora does not like missing links
shutil.rmtree("libexec", ignore_errors=True)

# Canonical Antora paths
os.makedirs("modules/ROOT/images", exist_ok=True)
os.makedirs("modules/ROOT/pages", exist_ok=True)

# Better naming
shutil.move("test/README.asciidoc", "test/tests.asciidoc")

# The main page
shutil.move("README.asciidoc", "index.asciidoc")

# We want this in the documentation as well
shutil.move("VIMTOKAK", "VIMTOKAK.asciidoc")

# Put necessary images to the Antora canonical directory
for gif_file in glob.glob("doc/*.gif"):
    shutil.copy(gif_file, "modules/ROOT/images/")

# Create directories structure matching the project's original structure
for asciidoc_file in glob.glob("**/*.asciidoc", recursive=True):
    file_dir = os.path.dirname(asciidoc_file)
    page_dir = os.path.join("modules/ROOT/pages", file_dir)
    os.makedirs(page_dir, exist_ok=True)

    # Take all `.asciidoc` files and put them into Antora's canonical directory
    # with the canonical `.adoc` file extension
    adoc = pathlib.Path(asciidoc_file).stem + ".adoc"
    dest = os.path.join(page_dir, adoc)
    shutil.copy(asciidoc_file, dest)

# Fix images
for adoc_file in glob.glob("modules/ROOT/pages/**/*.adoc", recursive=True):
    with open(adoc_file, "r") as f:
        content = f.read()
    content = content.replace("image::doc/", "image::")
    with open(adoc_file, "w") as f:
        f.write(content)

# Fix links using another Python script.
for adoc in glob.glob("modules/ROOT/pages/**/*.adoc", recursive=True):
    linksfixer.fix_file(adoc)


# Crafted manually from all the files at the time of writing the script.
# TODO: Generate automatically.
nav_content = """
* xref:index.adoc[Getting Started]
* xref:doc/pages/commands.adoc[Commands]
* xref:doc/pages/expansions.adoc[Expansions]
* xref:doc/pages/execeval.adoc[Exec Eval]
* xref:doc/pages/scopes.adoc[Scopes]
* xref:doc/pages/faces.adoc[Faces]
* xref:doc/pages/buffers.adoc[Buffers]
* xref:doc/pages/registers.adoc[Registers]
* xref:doc/pages/mapping.adoc[Mapping]
* xref:doc/pages/hooks.adoc[Hooks]
* xref:doc/pages/command-parsing.adoc[Command Parsing]
* xref:doc/pages/keys.adoc[Keys]
* xref:doc/pages/regex.adoc[Regex]
* xref:doc/pages/options.adoc[Options]
* xref:doc/pages/highlighters.adoc[Highlighters]
* xref:doc/pages/modes.adoc[Modes]
* xref:doc/pages/keymap.adoc[KEYMAP]
* xref:doc/pages/faq.adoc[FAQ]
* xref:doc/pages/changelog.adoc[Changelog]
* xref:doc/design.adoc[Design]
* xref:doc/coding-style.adoc[Coding Style]
* xref:doc/writing_scripts.adoc[Writing Scripts]
* xref:doc/json_ui.adoc[JSON UI]
* xref:doc/autoedit.adoc[Autoedit]
* xref:doc/interfacing.adoc[Interfacing]
* xref:rc/tools/lint.adoc[Linting]
* xref:rc/tools/autorestore.adoc[Autorestore]
* xref:rc/tools/doc.adoc[Doc]
* xref:test/tests.adoc[Tests]
* xref:VIMTOKAK.adoc[Vi(m) to Kakoune]
"""

with open("modules/ROOT/nav.adoc", "w") as f:
    f.write(nav_content)

# Antora's component description file
antora_yml_content = """
name: Kakoune
nav:
  - modules/ROOT/nav.adoc
title: Kakoune
version: latest
"""

with open("antora.yml", "w") as f:
    f.write(antora_yml_content)

# Antora playbook file
antora_playbook_content = """
asciidoc:
  attributes:

    # Do not complain on missing attributes,
    # TODO: fix and turn to a fatal warning
    attribute-missing: skip

    # To fix links
    idprefix: ""

    # To fix links
    idseparator: "-"

    # Better to be reproducible, in general
    reproducible: true

    # More convenient to turn sections to IDs
    sectids: true

    # More convenient to have sections as links
    sectlinks: true

    # Do not want to miss something
    sectnumlevels: 5

    # More convenient to number the sections
    sectnums: true

    # Do not want to miss something
    toclevels: 5

  sourcemap: true

content:
  sources:
  - url: ./
    branches: ["HEAD"]

runtime:
  cache_dir: ./cache # More convenient for the development
  fetch: true # More convenient for the development

  log:
    failure_level: fatal
    level: warn

output:
  clean: true # More convenient for the development
  dir: ./build # Simpler to have it explicit in code

site:
  title: Kakoune Docs

ui:
  bundle:
    url: https://gitlab.com/antora/antora-ui-default/-/jobs/artifacts/HEAD/raw/build/ui-bundle.zip?job=bundle-stable
"""

with open("antora-playbook.yml", "w") as f:
    f.write(antora_playbook_content)

# Finally, generate the documentation,
# will saved to the output directory specified in the antora-playbook.yml
subprocess.run(["antora", "generate", "antora-playbook.yml"])
subprocess.run(["xdg-open", "./build/Kakoune/latest/index.html"])
