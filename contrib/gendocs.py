#!/usr/bin/env python3

# This script generates a static documentation web site
# by parsing the `**/*.asiidoc` files from the repository.
#
# Dependencies:
# * Python 3
# * `xdg-open` for opening the final result in a web browser.
# * `antora` - a static documentation web site generator,
#     https://docs.antora.org/antora/latest
#
# Usage:
# ```console
# $ ./contrib/gendocs.py
# ```
#
# After running it should open the generated web site in a browser.
#


from dataclasses import dataclass
from typing import Optional
import atexit
import glob
import itertools
import os
import pathlib
import re
import shutil
import subprocess

# Get the script directory.
script_dir = os.path.dirname(os.path.realpath(__file__))

# Switch to the projects root dir.
os.chdir(os.path.join(script_dir, ".."))


# Revert the changes upon script exit.
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


atexit.register(cleanup)


# Antora fails if the repo contains broken symbolic links.
shutil.rmtree("libexec", ignore_errors=True)

# Canonical Antora paths.
# See: https://docs.antora.org/antora/latest/standard-directories.
#      https://docs.antora.org/antora/latest/root-module-directory.
os.makedirs("modules/ROOT/images", exist_ok=True)
os.makedirs("modules/ROOT/pages", exist_ok=True)

# The file name is used for navigation links.
# Update so it reflects the content.
shutil.move("test/README.asciidoc", "test/tests.asciidoc")

# Update the filename so it reflects the content.
# The filename is used for navigation links.
shutil.move("README.asciidoc", "index.asciidoc")

# A useful documentation page.
# Add the `.asciidoc` extension to include it into the result.
shutil.move("VIMTOKAK", "VIMTOKAK.asciidoc")

# Put necessary images to the Antora canonical directory.
# See: https://docs.antora.org/antora/latest/images-directory.
for gif_file in glob.glob("doc/*.gif"):
    shutil.copy(gif_file, "modules/ROOT/images/")


# Fix links according to the Antora specification.
# See: https://docs.antora.org/antora/latest/page/xref.
def fix_links(path):
    @dataclass
    class Link:
        path: Optional[str]
        file: Optional[str]
        fragment: Optional[str]
        title: str

        def __init__(self, path, file, fragment, title):
            self.path = path
            self.file = file
            self.fragment = fragment
            self.title = title

    def dropwhile(predicate, string):
        return "".join(itertools.dropwhile(predicate, string))

    def dropwhileright(predicate, string):
        return "".join(reversed(list(itertools.dropwhile(predicate, reversed(string)))))

    def takewhile(predicate, string):
        return "".join(itertools.takewhile(predicate, string))

    def untag(string):
        no_opening = dropwhile(lambda c: c == "<", string)
        no_closing = dropwhileright(lambda c: c == ">", no_opening)
        return no_closing

    def parse(string):
        untagged = untag(string)
        prefix, title = untagged.split(",", 1)
        title = title.strip()
        fragment = dropwhile(lambda c: c != "#", prefix)
        fragment = fragment if fragment else None
        fragmentless = takewhile(lambda c: c != "#", prefix)
        segments = fragmentless.split("/")
        path, file = (
            ("/".join(segments[:-1]), segments[-1])
            if "/" in fragmentless
            else (None, fragmentless)
        )
        return Link(path, file, fragment, title)

    def render(link):
        if link.path and link.file and link.fragment == "#":
            return f"xref:{link.path}/{link.file}.adoc[{link.title}]"
        elif link.path and link.file and link.fragment:
            return f"xref:{link.path}/{link.file}.adoc{link.fragment}[{link.title}]"
        elif not link.path and link.file and link.fragment == "#":
            return f"xref:./{link.file}.adoc[{link.title}]"
        elif not link.path and link.file and link.fragment:
            return f"xref:./{link.file}.adoc{link.fragment}[{link.title}]"
        elif not link.path and link.file and not link.fragment:
            return f"<<{link.file},{link.title}>>"
        else:
            raise RuntimeError(f"Failed to render link: {link}")

    def process(m):
        string = m.group(0)
        return render(parse(string)) if "," in string else string

    content = None

    with open(path, "r") as file:
        content = file.read()

    # Fix image links according the Antora specification.
    # See: https://docs.antora.org/antora/latest/page/image-resource-id-examples.
    content = content.replace("image::doc/", "image::")

    with open(path, "w") as file:
        file.write(re.sub(r"<<[^>]+>>", process, content))


for source in glob.glob("**/*.asciidoc", recursive=True):
    # Create directories structure matching the project's original structure.
    # See: https://docs.antora.org/antora/latest/pages-directory.
    page_dir = os.path.join("modules/ROOT/pages", os.path.dirname(source))
    os.makedirs(page_dir, exist_ok=True)

    # Copy the `asciidoc` file into the Antora `pages` directory
    # with the mandatory `.adoc` filename extension.
    adoc = os.path.join(page_dir, pathlib.Path(source).stem + ".adoc")
    shutil.copy(source, adoc)

    fix_links(adoc)


# A navigation file for the sidebar.
# See: https://docs.antora.org/antora/latest/navigation/single-list.
#
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

# Antora component description file.
# See: https://docs.antora.org/antora/latest/component-version-descriptor.
antora_yml_content = """
name: Kakoune
nav:
  - modules/ROOT/nav.adoc
title: Kakoune
version: latest
"""

with open("antora.yml", "w") as f:
    f.write(antora_yml_content)

# Antora playbook file.
# See: https://docs.antora.org/antora/latest/playbook.
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
# results will be saved to the output directory
# as specified in the `antora-playbook.yml`.
subprocess.run(["antora", "generate", "antora-playbook.yml"])

subprocess.run(["xdg-open", "./build/Kakoune/latest/index.html"])
