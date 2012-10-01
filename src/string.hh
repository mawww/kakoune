#ifndef string_hh_INCLUDED
#define string_hh_INCLUDED

#include <string>
#include <iosfwd>
#include <boost/regex.hpp>

#include "memoryview.hh"
#include "units.hh"

namespace Kakoune
{

typedef int32_t Character;
typedef boost::regex Regex;

class String
{
public:
   String() {}
   String(const char* content) : m_content(content) {}
   String(std::string content) : m_content(std::move(content)) {}
   String(const String& string) = default;
   String(String&& string) = default;
   explicit String(char content) : m_content(std::string() + content) {}
   template<typename Iterator>
   String(Iterator begin, Iterator end) : m_content(begin, end) {}

   char      operator[](CharCount pos) const { return m_content[(int)pos]; }
   CharCount length() const { return m_content.length(); }
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

   String substr(CharCount pos, CharCount length = -1) const { return String(m_content.substr((int)pos, (int)length)); }
   String replace(const String& expression, const String& replacement) const;

   using iterator = std::string::const_iterator;

   iterator begin() const { return m_content.begin(); }
   iterator end()   const { return m_content.end(); }

   char     front() const { return m_content.front(); }
   char     back()  const { return m_content.back(); }

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

inline bool is_word(Character c)
{
    return (c >= '0' and c <= '9') or
           (c >= 'a' and c <= 'z') or
           (c >= 'A' and c <= 'Z') or
           c == '_';
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

