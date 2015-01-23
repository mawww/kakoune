import gdb.printing

class ArrayIterator:
    def __init__ (self, data, count):
        self.data = data
        self.count = count
        self.index = 0

    def __iter__ (self):
        return self

    def next (self):
        if self.index == self.count:
            raise StopIteration

        index = self.index
        self.index = self.index + 1
        return ('[%d]' % index, (self.data + index).dereference())

class ArrayView:
    """Print a ArrayView"""

    def __init__(self, val):
        self.val = val

    def display_hint(self):
        return 'array'

    def children(self):
        return ArrayIterator(self.val['m_pointer'], self.val['m_size'])

    def to_string(self):
        value_type = self.val.type.template_argument(0).unqualified().strip_typedefs()
        return "ArrayView<%s>" % (value_type)

class LineAndColumn:
    """Print a LineAndColumn"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        value_type = self.val.type.unqualified()
        return "%s(%s, %s)" % (value_type, self.val['line'], self.val['column'])

class BufferIterator:
    """ Print a BufferIterator"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        if self.val['m_buffer']['m_ptr'] != 0:
            return "buffer<%s>@(%s, %s)" % (self.val['m_buffer']['m_ptr'].dereference()['m_name'], self.val['m_coord']['line'], self.val['m_coord']['column'])
        else:
            return "buffer<none>@(%s, %s)" % (self.val['m_coord']['line'], self.val['m_coord']['column'])

class String:
    """ Print a String"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        std_str = gdb.lookup_type("std::string")
        return self.val.cast(std_str)

class StringView:
    """ Print a StringView"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        return "\"%s\"" % (self.val['m_data'].string("utf-8", "ignore", self.val['m_length']['m_value']))

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

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("kakoune")
    pp.add_printer('ArrayView',      '^Kakoune::ArrayView<.*>$',     ArrayView)
    pp.add_printer('LineAndColumn',  '^Kakoune::LineAndColumn<.*>$', LineAndColumn)
    pp.add_printer('ByteCoord',      '^Kakoune::ByteCoord$',         LineAndColumn)
    pp.add_printer('CharCoord',      '^Kakoune::CharCoord$',         LineAndColumn)
    pp.add_printer('BufferIterator', '^Kakoune::BufferIterator$',    BufferIterator)
    pp.add_printer('String',         '^Kakoune::String$',            String)
    pp.add_printer('StringView',     '^Kakoune::StringView$',        StringView)
    pp.add_printer('SharedString',   '^Kakoune::SharedString$',      StringView)
    pp.add_printer('Option',         '^Kakoune::Option$',            Option)
    pp.add_printer('LineCount',      '^Kakoune::LineCount$',         LineCount)
    pp.add_printer('CharCount',      '^Kakoune::CharCount$',         CharCount)
    pp.add_printer('ByteCount',      '^Kakoune::ByteCount$',         ByteCount)
    return pp

