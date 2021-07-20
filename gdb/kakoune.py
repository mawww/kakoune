import gdb.printing


class ArrayIterator:
    def __init__(self, data, count):
        self.data = data
        self.count = count
        self.index = 0

    def __iter__(self):
        return self

    def next(self):
        if self.index == self.count:
            raise StopIteration

        index = self.index
        self.index = self.index + 1
        return ('[%d]' % index, (self.data + index).dereference())

    def __next__(self):
        return self.next()


class ArrayView:
    """Print a ArrayView"""

    def __init__(self, val):
        self.val = val

    def display_hint(self):
        return 'array'

    def children(self):
        return ArrayIterator(self.val['m_pointer'], self.val['m_size'])

    def to_string(self):
        type = self.val.type.template_argument(0).unqualified().strip_typedefs()
        return "ArrayView<%s>" % (type)


class LineAndColumn:
    """Print a LineAndColumn"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        value_type = self.val.type.unqualified()
        return "%s(%s, %s)" % (value_type, self.val['line'],
                               self.val['column'])


class BufferCoordAndTarget:
    """Print a BufferCoordAndTarget"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        value_type = self.val.type.unqualified()
        return "%s(%s, %s, %s)" % (value_type, self.val['line'],
                                   self.val['column'], self.val['target'])


class BufferIterator:
    """ Print a BufferIterator"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        line = self.val['m_coord']['line']
        column = self.val['m_coord']['column']
        if self.val['m_buffer']['m_ptr'] != 0:
            buf = self.val['m_buffer']['m_ptr'].dereference()['m_name']
            return "buffer<%s>@(%s, %s)" % (buf, line, column)
        else:
            return "buffer<none>@(%s, %s)" % (line, column)


class String:
    """ Print a String"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        data = self.val["m_data"]
        if (data["u"]["s"]["size"] & 1) != 1:
            ptr = data["u"]["l"]["ptr"]
            len = data["u"]["l"]["size"]
        else:
            ptr = data["u"]["s"]["string"]
            len = data["u"]["s"]["size"] >> 1
        return "\"%s\"" % (ptr.string("utf-8", "ignore", len))


class StringView:
    """ Print a StringView"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        len = self.val['m_length']['m_value']
        return "\"%s\"" % (self.val['m_data'].string("utf-8", "ignore", len))


class StringDataPtr:
    """ Print a RefPtr<StringData>"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        ptr = self.val['m_ptr']
        str_type = gdb.lookup_type("char").pointer()
        len = ptr.dereference()['length']
        refcount = ptr.dereference()['refcount']
        content = (ptr + 1).cast(str_type).string("utf-8", "ignore", len)
        return "\"%s\" (ref:%d)" % (content.replace("\n", "\\n"), refcount)


class RefPtr:
    """ Print a RefPtr"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        ptr = self.val['m_ptr']
        return "\"refptr %s\"" % (ptr)


class Option:
    """ Print a Option"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val["m_value"]


class CharCount:
    """Print a CharCount"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val["m_value"]


class ColumnCount:
    """Print a ColumnCount"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val["m_value"]


class ByteCount:
    """Print a ByteCount"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val["m_value"]


class LineCount:
    """Print a LineCount"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return self.val["m_value"]


class Color:
    """Print a Color"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        named_color = gdb.lookup_type("Kakoune::Color::NamedColor")
        if self.val["color"] == named_color["Kakoune::Color::RGB"].enumval:
            return "%s #%02x%02x%02x" % (self.val["color"], self.val["r"],
                                         self.val["g"], self.val["b"])
        else:
            return self.val["color"]

class Regex:
    """Print a Regex"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "regex%s" % (self.val["m_str"])


def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("kakoune")
    pp.add_printer('ArrayView', '^Kakoune::(Const)?ArrayView<.*>$', ArrayView)
    pp.add_printer('LineAndColumn', '^Kakoune::(Buffer|Display)Coord$', LineAndColumn)
    pp.add_printer('BufferCoordAndTarget', '^Kakoune::BufferCoordAndTarget$', BufferCoordAndTarget)
    pp.add_printer('BufferIterator', '^Kakoune::BufferIterator$', BufferIterator)
    pp.add_printer('String', '^Kakoune::String$', String)
    pp.add_printer('StringView', '^Kakoune::(StringView|SharedString)$', StringView)
    pp.add_printer('StringDataPtr', '^Kakoune::StringDataPtr$', StringDataPtr)
    pp.add_printer('StringDataPtr', '^Kakoune::RefPtr<Kakoune::StringData,.*>$', StringDataPtr)
    pp.add_printer('RefPtr', '^Kakoune::RefPtr<.*>$',  RefPtr)
    pp.add_printer('Option', '^Kakoune::Option$', Option)
    pp.add_printer('LineCount', '^Kakoune::LineCount$', LineCount)
    pp.add_printer('CharCount', '^Kakoune::CharCount$', CharCount)
    pp.add_printer('ColumnCount', '^Kakoune::ColumnCount$', ColumnCount)
    pp.add_printer('ByteCount', '^Kakoune::ByteCount$', ByteCount)
    pp.add_printer('Color', '^Kakoune::Color$', Color)
    pp.add_printer('Regex', '^Kakoune::Regex$', Regex)
    return pp
