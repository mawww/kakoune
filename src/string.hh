#ifndef string_hh_INCLUDED
#define string_hh_INCLUDED

#include <string>
#include <iosfwd>
#include <boost/regex.hpp>

#include "memoryview.hh"
#include "units.hh"

namespace Kakoune
{

typedef wchar_t Character;
typedef boost::basic_regex<Character> Regex;

class String
{
public:
   String() {}
   String(const char* content) : m_content(content) {}
   String(std::string content) : m_content(std::move(content)) {}
   String(const String& string) = default;
   String(String&& string) = default;
   explicit String(Character content) : m_content(std::string() + (char)content) {}
   template<typename Iterator>
   String(Iterator begin, Iterator end) : m_content(begin, end) {}

   Character operator[](CharCount pos) const { return static_cast<Character>(m_content[(int)pos]); }
   CharCount length() const { return m_content.length(); }
   bool      empty()  const { return m_content.empty(); }

   bool      operator== (const String& other) const { return m_content == other.m_content; }
   bool      operator!= (const String& other) const { return m_content != other.m_content; }
   bool      operator<  (const String& other) const { return m_content < other.m_content;  }

   String&   operator=  (const String& other)       { m_content = other.m_content; return *this; }
   String&   operator=  (String&& other)            { m_content = std::move(other.m_content); return *this; }

   String    operator+  (const String& other) const { return String(m_content + other.m_content); }
   String&   operator+= (const String& other)       { m_content += other.m_content; return *this; }

   String    operator+  (Character character) const { return String(m_content + (char)character); }
   String&   operator+= (Character character)       { m_content += (char)character; return *this; }

   memoryview<char> data()  const { return memoryview<char>(m_content.data(), m_content.size()); }
   const char*      c_str() const { return m_content.c_str(); }

   String substr(CharCount pos, CharCount length = -1) const { return String(m_content.substr((int)pos, (int)length)); }
   String replace(const String& expression, const String& replacement) const;

   class iterator
   {
   public:
       typedef Character         value_type;
       typedef const value_type* pointer;
       typedef const value_type& reference;
       typedef size_t            difference_type;
       typedef std::random_access_iterator_tag iterator_category;

       iterator() {}
       iterator(const std::string::const_iterator& it) : m_iterator(it) {}

       Character operator*()
       { return static_cast<Character>(*m_iterator); }

       iterator& operator++ () { ++m_iterator; return *this; }
       iterator& operator-- () { --m_iterator; return *this; }

       iterator operator+ (size_t size) { return iterator(m_iterator + size); }
       iterator operator- (size_t size) { return iterator(m_iterator - size); }

       iterator& operator+= (size_t size) { m_iterator += size; return *this; }
       iterator& operator-= (size_t size) { m_iterator -= size; return *this; }

       size_t operator- (const iterator& other) const { return m_iterator - other.m_iterator; }

       bool operator== (const iterator& other) const { return m_iterator == other.m_iterator; }
       bool operator!= (const iterator& other) const { return m_iterator != other.m_iterator; }
       bool operator<  (const iterator& other) const { return m_iterator < other.m_iterator; }
       bool operator<= (const iterator& other) const { return m_iterator <= other.m_iterator; }
       bool operator>  (const iterator& other) const { return m_iterator > other.m_iterator; }
       bool operator>= (const iterator& other) const { return m_iterator >= other.m_iterator; }

   private:
       std::string::const_iterator m_iterator;
   };

   iterator  begin() const { return iterator(m_content.begin()); }
   iterator  end()   const { return iterator(m_content.end()); }

   Character front() const { return Character(m_content.front()); }
   Character back()  const { return Character(m_content.back()); }

   size_t    hash() const { return std::hash<std::string>()(m_content); }

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

inline String operator+(Character lhs, const String& rhs)
{
    return String(lhs) + rhs;
}

String int_to_str(int value);
int    str_to_int(const String& str);
std::vector<String> split(const String& str, Character separator);

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

