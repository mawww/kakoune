#ifndef string_hh_INCLUDED
#define string_hh_INCLUDED

#include <string>
#include <iosfwd>
#include <climits>
#include <boost/regex.hpp>

#include "memoryview.hh"
#include "units.hh"
#include "utf8.hh"

namespace Kakoune
{

typedef boost::regex Regex;

class String
{
public:
   String() {}
   String(const char* content) : m_content(content) {}
   String(std::string content) : m_content(std::move(content)) {}
   String(const String& string) = default;
   String(String&& string) = default;
   explicit String(char content, size_t count = 1) : m_content(count, content) {}
   template<typename Iterator>
   String(Iterator begin, Iterator end) : m_content(begin, end) {}

   char      operator[](ByteCount pos) const { return m_content[(int)pos]; }
   ByteCount length() const { return m_content.length(); }
   CharCount char_length() const { return utf8::distance(begin(), end()); }
   ByteCount byte_count_to(CharCount count) const { return utf8::advance(begin(), end(), (int)count) - begin(); }
   CharCount char_count_to(ByteCount count) const { return utf8::distance(begin(), begin() + (int)count); }
   bool      empty()  const { return m_content.empty(); }

   bool      operator== (const String& other) const { return m_content == other.m_content; }
   bool      operator!= (const String& other) const { return m_content != other.m_content; }
   bool      operator<  (const String& other) const { return m_content < other.m_content;  }

   String&   operator=  (const String& other)       { m_content = other.m_content; return *this; }
   String&   operator=  (String&& other)            { m_content = std::move(other.m_content); return *this; }

   String    operator+  (const String& other) const { return String(m_content + other.m_content); }
   String&   operator+= (const String& other)       { m_content += other.m_content; return *this; }

   String    operator+  (char c) const { return String(m_content + c); }
   String&   operator+= (char c)       { m_content += c; return *this; }

   memoryview<char> data()  const { return memoryview<char>(m_content.data(), m_content.size()); }
   const char*      c_str() const { return m_content.c_str(); }

   String substr(ByteCount pos, ByteCount length = -1) const { return String(m_content.substr((int)pos, (int)length)); }
   String substr(CharCount pos, CharCount length = INT_MAX) const
   {
       auto b = utf8::advance(begin(), end(), (int)pos);
       auto e = utf8::advance(b, end(), (int)length);
       return String(b,e);
   }
   String replace(const String& expression, const String& replacement) const;

   using iterator = std::string::const_iterator;

   iterator begin() const { return m_content.begin(); }
   iterator end()   const { return m_content.end(); }

   char     front() const { return m_content.front(); }
   char     back()  const { return m_content.back(); }
   char&    front()       { return m_content.front(); }
   char&    back()        { return m_content.back(); }

   size_t   hash() const { return std::hash<std::string>()(m_content); }

   inline friend std::ostream& operator<<(std::ostream& os, const String& str)
   {
       return os << str.m_content;
   }

   enum { npos = -1 };

private:
   std::string m_content;
};

inline String operator+(const char* lhs, const String& rhs)
{
    return String(lhs) + rhs;
}

inline String operator+(char lhs, const String& rhs)
{
    return String(lhs) + rhs;
}

String int_to_str(int value);
int    str_to_int(const String& str);
std::vector<String> split(const String& str, char separator);

inline String operator"" _str(const char* str, size_t)
{
    return String(str);
}

inline String codepoint_to_str(Codepoint cp)
{
    std::string str;
    auto it = back_inserter(str);
    utf8::dump(it, cp);
    return String(str);
}

}

namespace std
{
    template<>
    struct hash<Kakoune::String>
    {
        size_t operator()(const Kakoune::String& str) const
        {
            return str.hash();
        }
    };
}

#endif // string_hh_INCLUDED

