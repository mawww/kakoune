#!/usr/bin/env bash

# This script searches for `**/*.asciidoc` files in the repository,
# and uses them to generate a static documentation website.

# Requirements:
# - https://github.com/sharkdp/fd - TODO: switch to `find`
# - https://github.com/VirtusLab/scala-cli - TODO: switch to `awk`
# - GNU sed
# - https://gitlab.com/antora/antora

# From https://stackoverflow.com/a/246128
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# DEBUG=1 ./contrib/gendocs.sh will trace the script's execution
if [[ -n "$DEBUG" ]]
then
  set -x
fi

set -euo pipefail
trap "echo 'error: Script failed: see failed command above'" ERR

# Switch to the projects root dir
cd "$SCRIPT_DIR/.."

# Idempotency
cleanup() {
  mv test/tests.asciidoc test/README.asciidoc
  mv index.asciidoc README.asciidoc
  mv VIMTOKAK.asciidoc VIMTOKAK
}

trap cleanup EXIT

# Antora does not like missing links
rm -rf libexec

# Canonical Antora paths
mkdir -p modules/ROOT/{images,pages}

# Better naming
mv test/README.asciidoc test/tests.asciidoc

# The main page
mv README.asciidoc index.asciidoc

# We want this in the documentation as well
mv VIMTOKAK VIMTOKAK.asciidoc

# Put necessary images to the Antora canonical directory
cp doc/*.gif modules/ROOT/images/

# Create directories structure matching the project's original structure
fd --extension=asciidoc --exec mkdir -p 'modules/ROOT/pages/{//}'

# Take all `.asciidoc` files and put them into Antora's canonical directory with the canonical `.adoc` file extension
fd --extension=asciidoc --exec cp {} 'modules/ROOT/pages/{.}.adoc'

# Fix images
fd --extension=adoc --exec \
  sed --in-place --regexp-extended 's%image::doc/%image::%' '{}'

# Fix links using ScalaCLI script.
# Unfortunately it was way too hard to achieve using `awk` or `sed`.
cat > links-fixer.scala <<EOF
//> using scala 3.3.1
//> using options -Werror, -Wnonunit-statement, -Wunused:all, -Wvalue-discard, -Yno-experimental, -Ysafe-init, -deprecation, -feature, -new-syntax, -unchecked,

import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.Paths
import scala.jdk.CollectionConverters.*
import scala.util.chaining.*

final case class Link(
    path: Option[String],
    file: Option[String],
    fragment: Option[String],
    title: String,
)

object LinksFixer:

  def main(files: Array[String]): Unit = files
    .foreach(f => fixFile(Paths.get(f)))

  private def fixFile(path: Path): Unit =
    val file = Files.readAllLines(path).asScala.mkString("\n")
    val _ = Files
      .writeString(path, "<<[^>]+>>".r.replaceAllIn(file, m => fix(m.matched)))

  private def fix(string: String): String =
    if string.contains(",") then render(parse(string)) else string

  private def parse(string: String): Link =
    val trimmed = string.dropWhile(_ == '<').reverse.dropWhile(_ == '>').reverse
    val Array(prefix, title) = trimmed.split(',')
    val fragment = prefix
      .dropWhile(_ != '#')
      .trim
      .pipe(s => Option.when(s.nonEmpty)(s))
    val fragmentless = prefix.takeWhile(_ != '#')
    val (path, file) =
      if fragmentless.contains("/") then
        fragmentless.split('/').pipe(s => (Some(s.init.mkString("/")), s.last))
      else (None, fragmentless)
    Link(path, Some(file), fragment, title.trim)

  private def render(link: Link): String = link match
    case Link(Some(path), Some(file), Some("#"), title) =>
      s"xref:\$path/\$file.adoc[\$title]"
    case Link(Some(path), Some(file), Some(fragment), title) =>
      s"xref:\$path/\$file.adoc\$fragment[\$title]"
    case Link(None, Some(file), Some("#"), title) =>
      s"xref:./\$file.adoc[\$title]"
    case Link(None, Some(file), Some(fragment), title) =>
      s"xref:./\$file.adoc\$fragment[\$title]"
    case Link(None, Some(file), None, title) => s"<<\$file,\$title>>"
    case _ => throw new RuntimeException(s"Failed to render link: \$link")
EOF

fd --extension=adoc --absolute-path --exec-batch \
  scala-cli run ./links-fixer.scala -- '{}'


# Crafted manually from all the files at the time of writing the script.
# TODO: Generate automatically.
cat > modules/ROOT/nav.adoc <<EOF
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
EOF

# Antora's component description file
cat > antora.yml <<EOF
name: Kakoune
nav:
  - modules/ROOT/nav.adoc
title: Kakoune
version: latest
EOF

# Antora playbook file
cat > antora-playbook.yml <<EOF
asciidoc:
  attributes:
    attribute-missing: skip # Do not complain on missing attributes, TODO: fix and turn to a fatal warning
    idprefix: "" # To fix links
    idseparator: "-" # To fix links
    reproducible: true # Better to be reproducible, in general
    sectids: true # More convenient to turn sections to IDs
    sectlinks: true # More convenient to have sections as links
    sectnumlevels: 5 # Do not want to miss something
    sectnums: true # More convenient to number the sections
    toclevels: 5 # Do not want to miss something
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
EOF


# Finally, generate the documentation, will saved to the output directory specified in the antora-playbook.yml
antora generate antora-playbook.yml
xdg-open ./build/Kakoune/latest/index.html
