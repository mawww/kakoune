#!/usr/bin/env python3

import sys
import logging
import pathlib
import argparse
import textwrap

from collections import namedtuple

from mako.template import Template
from mako.runtime import Context


g_adoc_template = """
= ${stem.title()}
% if aliases:

== Aliases
% for alias in aliases:

*${alias.name}*::
	_default_ ${alias.command} +
% endfor
% endif
% if commands:

== Commands
% for command in commands:

*${command.name}*::
	_parameters_ ${command.params} +
% if command.docstring:
${indent(command.docstring)}
% endif
% endfor
% endif
% if faces:

== Faces
% for face in faces:

*${face.name}*::
	_default_ ${face.value} +
% endfor
% endif
% if modules:

== Modules

% for module in modules:
*${module.name}*::
% endfor
% endif
% if options:

== Options
% for option in options:

*${option.name}* `${option.type}`::
% if option.value:
	_default_ ${option.value} +
% endif
% if option.docstring:
	${option.docstring}
% endif
% endfor
% endif
% if usermodes:

== User Modes
% for usermode in usermodes:

*${usermode.name}*::
% endfor
% endif
% if sources:

== Source

% for source in sources:
*${source.path}*::
% endfor
% endif
"""

alias_t = namedtuple("alias_t", "name command")
command_t = namedtuple("command_t", "name params docstring")
face_t = namedtuple("face_t", "name value")
module_t = namedtuple("module_t", "name")
option_t = namedtuple("option_t", "name type value docstring")
source_t = namedtuple("source_t", "path")
usermode_t = namedtuple("usermode_t", "name")


class Node(object):
    def __init__(self, parent=None, **kwargs):
        super().__init__()

        self.node_name = "node"
        self.parent = parent
        self.children = []
        self.data = kwargs

    def __str__(self):
        return "%s (%s)" % (self.node_name, ", ".join(["%s=%s" % (k, v) for k, v in self.data.items()]))


class NodeAlias(Node):
    def __init__(self, parent, name, command):
        super().__init__(parent, name=name, command=command)

        self.node_name = "Alias"


class NodeCommand(Node):
    def __init__(self, parent, name, params, docstring):
        super().__init__(parent, name=name, params=params, docstring=docstring)

        self.node_name = "Command"


class NodeFace(Node):
    def __init__(self, parent, name, value):
        super().__init__(parent, name=name, value=value)

        self.node_name = "Face"


class NodeModule(Node):
    def __init__(self, parent, name):
        super().__init__(parent, name=name)

        self.node_name = "Module"


class NodeOption(Node):
    def __init__(self, parent, name, type, value, docstring):
        super().__init__(parent, name=name, type=type, value=value, docstring=docstring)

        self.node_name = "Option"


class NodeSource(Node):
    def __init__(self, parent, path):
        super().__init__(parent, path=path)

        self.node_name = "Source"


class NodeUserMode(Node):
    def __init__(self, parent, name):
        super().__init__(parent, name=name)

        self.node_name = "UserMode"


def pretty_print(node, level=0):
    print("%s%s" % (" " * (level * 4), node))

    for child in node.children:
        pretty_print(child, level + 1)


def generate_adoc(ast, path_output="adoc", stem="/home/fle/misc/kakoune"):
    def normalise_path_source_file(path, stem):
        return pathlib.Path(path[len(stem):].lstrip("/"))

    def path_to_adoc(path_output, path):
        return path_output.joinpath(path.parent, path.stem + ".asciidoc")

    def _generate_adoc_source(node, path_output, template):
        assert isinstance(node, NodeSource)
        assert node.data["path"].startswith(stem)

        path_source_file = normalise_path_source_file(node.data["path"], stem)
        path_docfile = path_to_adoc(path_output, path_source_file)

        path_docfile.parent.mkdir(parents=True, exist_ok=True)

        nodes_source = [node
                            for node in node.children if isinstance(node, NodeSource)]
        display_nodes_source = [source_t(path=node.data["path"][len(stem):].lstrip("/"))
                                    for node in nodes_source]
        display_nodes_alias = [alias_t(name=node.data["name"], command=node.data["command"])
                                   for node in node.children if isinstance(node, NodeAlias)]
        display_nodes_command = [command_t(name=node.data["name"], params=node.data["params"], docstring=node.data["docstring"])
                                     for node in node.children if isinstance(node, NodeCommand)]
        display_nodes_face = [face_t(name=node.data["name"], value=node.data["value"])
                                  for node in node.children if isinstance(node, NodeFace)]
        display_nodes_module = [module_t(name=node.data["name"])
                                   for node in node.children if isinstance(node, NodeModule)]
        display_nodes_option = [option_t(name=node.data["name"], type=node.data["type"], value=node.data["value"], docstring=node.data["docstring"])
                                    for node in node.children if isinstance(node, NodeOption)]
        display_nodes_usermodes = [usermode_t(name=node.data["name"])
                                       for node in node.children if isinstance(node, NodeUserMode)]

        with open(path_docfile, "w") as fout:
            context = Context(fout,
                              stem=path_docfile.stem,
                              aliases=sorted(display_nodes_alias, key=lambda x: x.name),
                              commands=sorted(display_nodes_command, key=lambda x: x.name),
                              faces=sorted(display_nodes_face, key=lambda x: x.name),
                              modules=sorted(display_nodes_module, key=lambda x: x.name),
                              options=sorted(display_nodes_option, key=lambda x: x.name),
                              sources=display_nodes_source,
                              usermodes=sorted(display_nodes_usermodes, key=lambda x: x.name),
                              indent=lambda x: textwrap.indent(x, "\t"))
            template.render_context(context)

        for node in nodes_source:
            _generate_adoc_source(node, path_output, template)

    path_output = pathlib.Path(path_output).resolve()

    path_output.mkdir(parents=True, exist_ok=True)

    template = Template(g_adoc_template.strip("\n"))

    for child in ast.children:
        _generate_adoc_source(child, path_output, template)


class CliOptions(argparse.Namespace):
    def __init__(self, args):
        parser = argparse.ArgumentParser(description="Tainted data parser")

        parser.add_argument("-d", "--debug", action="store_true", help="Display debug messages")
        parser.add_argument("-v", "--verbose", action="store_true", help="Display informational messages")
        parser.add_argument("path", help="Path to the file to parse")

        parser.parse_args(args, self)


def main(av):
    cli_options = CliOptions(av[1:])

    logging_level = logging.WARN
    if cli_options.debug:
        logging_level = logging.DEBUG
    elif cli_options.verbose:
        logging_level = logging.INFO
    logging.basicConfig(level=logging_level,
                        format="[%(asctime)s][%(levelname)s]: %(message)s")

    logging.debug("file: %s", cli_options.path)

    ast = Node()
    head = ast
    with open(cli_options.path, "r") as fin:
        last_node = None
        for line in fin:
            line = line.rstrip("\n")

            logging.debug("line: %s", line)

            sline = line.split("\t")
            if sline:
                if sline[0] == "SOURCE":
                    assert len(sline) == 2
                    node = NodeSource(parent=head, path=sline[1])
                    head.children.append(node)
                    head = node
                    last_node = node
                elif sline[0] == "ENDSOURCE":
                    head = head.parent
                    last_node = head
                elif sline[0] == "ALIAS":
                    assert len(sline) == 3
                    node = NodeAlias(parent=head, name=sline[1], command=sline[2])
                    head.children.append(node)
                    last_node = node
                elif sline[0] == "COMMAND":
                    assert len(sline) == 4
                    node = NodeCommand(parent=head, name=sline[1], params=sline[2], docstring=sline[3])
                    head.children.append(node)
                    last_node = node
                elif sline[0] == "FACE":
                    assert len(sline) == 3
                    node = NodeFace(parent=head, name=sline[1], value=sline[2])
                    head.children.append(node)
                    last_node = node
                elif sline[0] == "USERMODE":
                    assert len(sline) == 2
                    node = NodeUserMode(parent=head, name=sline[1])
                    head.children.append(node)
                    last_node = node
                elif sline[0] == "OPTION":
                    assert len(sline) == 5
                    node = NodeOption(parent=head,
                                      name=sline[1], type=sline[2], value=sline[3], docstring=sline[4])
                    head.children.append(node)
                    last_node = node
                elif sline[0] == "MODULE":
                    assert len(sline) == 2
                    node = NodeModule(parent=head, name=sline[1])
                    head.children.append(node)
                    last_node = node
                elif "docstring" in last_node.data:
                    if last_node.data["docstring"]:
                        last_node.data["docstring"] += "\n"
                    last_node.data["docstring"] += line

    pretty_print(ast)
    generate_adoc(ast)

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
