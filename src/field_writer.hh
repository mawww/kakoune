#ifndef field_writer_hh_INCLUDED
#define field_writer_hh_INCLUDED

#include "color.hh"
#include "display_buffer.hh"
#include "hash_map.hh"
#include "optional.hh"
#include "string.hh"
#include "vector.hh"

#include <limits>

namespace Kakoune
{

using RemoteBuffer = Vector<char, MemoryDomain::Remote>;

// Used to disambiguate types
struct Raw {
    Raw(const RemoteBuffer& d) : data{d} {}
    const RemoteBuffer& data;
};

template<typename VariableSizeType>
class FieldWriter
{
public:
    FieldWriter(RemoteBuffer& buffer)
        : m_buffer{buffer}
    {
    }

    template<typename ...Args>
    void write(Args&&... args)
    {
        (write_field(std::forward<Args>(args)), ...);
    }

private:
    void write_raw(const char* val, size_t size)
    {
        m_buffer.insert(m_buffer.end(), val, val + size);
    }

    template<typename T>
    void write_field(const T& val)
    {
        static_assert(std::is_trivially_copyable<T>::value, "");
        write_raw((const char*)&val, sizeof(val));
    }

    void write_field(StringView str)
    {
        write_field(convert_size(str.length()));
        write_raw(str.data(), (int)str.length());
    }

    void write_field(const char* str)
    {
        write_field(StringView{str});
    }

    void write_field(const String& str)
    {
        write_field(StringView{str});
    }

    template<typename T>
    void write_field(ConstArrayView<T> view)
    {
        write_field(convert_size(view.size()));
        for (auto& val : view)
            write_field(val);
    }

    template<typename T, MemoryDomain domain>
    void write_field(const Vector<T, domain>& vec)
    {
        write_field(ConstArrayView<T>(vec));
    }

    template<typename Key, typename Val, MemoryDomain domain>
    void write_field(const HashMap<Key, Val, domain>& map)
    {
        write_field(convert_size(map.size()));
        for (auto& val : map)
        {
            write_field(val.key);
            write_field(val.value);
        }
    }

    template<typename T>
    void write_field(const Optional<T>& val)
    {
        write_field((bool)val);
        if (val)
            write_field(*val);
    }

    void write_field(Color color)
    {
        write_field(color.color);
        if (color.color == Color::RGB)
        {
            write_field(color.r);
            write_field(color.g);
            write_field(color.b);
        }
    }

    void write_field(const DisplayAtom& atom)
    {
        write_field(atom.content());
        write_field(atom.face);
    }

    void write_field(const DisplayLine& line)
    {
        write_field(line.atoms());
    }

    void write_field(const DisplayBuffer& display_buffer)
    {
        write_field(display_buffer.lines());
    }

    void write_field(const Raw& raw)
    {
        write_raw(raw.data.data(), raw.data.size());
    }

private:
    static inline VariableSizeType convert_size(ByteCount size)
    {
        kak_assert(size >= 0);
        kak_assert(unsigned(int(size)) <= std::numeric_limits<VariableSizeType>::max());
        return VariableSizeType(int(size));
    }

    RemoteBuffer& m_buffer;
};

typedef FieldWriter<uint16_t> NinePFieldWriter;

}

#endif // field_writer_hh_INCLUDED

