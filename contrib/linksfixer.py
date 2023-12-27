from dataclasses import dataclass
from typing import Optional
import itertools
import re


@dataclass
class _Link:
    path: Optional[str]
    file: Optional[str]
    fragment: Optional[str]
    title: str

    def __init__(self, path, file, fragment, title):
        self.path = path
        self.file = file
        self.fragment = fragment
        self.title = title


def fix_file(path):
    with open(path, "r") as file:
        content = file.read()

    def fix(m):
        string = m.group(0)
        result = _render(_parse(string)) if "," in string else string
        return result

    with open(path, "w") as file:
        file.write(re.sub(r"<<[^>]+>>", fix, content))


def _str_dropwhile(predicate, string):
    return "".join(itertools.dropwhile(predicate, string))


def _str_dropwhileright(predicate, string):
    return "".join(
        reversed(list(itertools.dropwhile(predicate, reversed(string))))
    )


def _str_takewhile(predicate, string):
    return "".join(itertools.takewhile(predicate, string))


def untag(string):
    no_opening = _str_dropwhile(lambda c: c == "<", string)
    no_closing = _str_dropwhileright(lambda c: c == ">", no_opening)
    return no_closing


def _parse(string):
    trimmed = untag(string)
    prefix, title = trimmed.split(",", 1)
    fragment = _str_dropwhile(lambda c: c != "#", prefix)
    fragment = fragment if fragment else None
    fragmentless = _str_takewhile(lambda c: c != "#", prefix)
    segments = fragmentless.split("/")
    path, file = (
        ("/".join(segments[:-1]), segments[-1])
        if "/" in fragmentless
        else (None, fragmentless)
    )
    return _Link(path, (file), fragment, title.strip())


def _render(link):
    res = None
    if link.path and link.file and link.fragment == "#":
        res = f"xref:{link.path}/{link.file}.adoc[{link.title}]"
    elif link.path and link.file and link.fragment:
        res = f"xref:{link.path}/{link.file}.adoc{link.fragment}[{link.title}]"
    elif not link.path and link.file and link.fragment == "#":
        res = f"xref:./{link.file}.adoc[{link.title}]"
    elif not link.path and link.file and link.fragment:
        res = f"xref:./{link.file}.adoc{link.fragment}[{link.title}]"
    elif not link.path and link.file and not link.fragment:
        res = f"<<{link.file},{link.title}>>"
    else:
        raise RuntimeError(f"Failed to render link: {link}")
    return res
