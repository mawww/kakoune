#ifndef word_splitter_hh_INCLUDED
#define word_splitter_hh_INCLUDED

#include "string.hh"
#include "array_view.hh"

namespace Kakoune
{

struct WordSplitter
{
    static constexpr ByteCount max_word_len = 100;

    struct Iterator
    {
        Iterator(const char* begin, const WordSplitter& splitter)
            : m_word_begin{begin}, m_word_end{begin}, m_splitter{&splitter}
        { operator++(); }

        StringView operator*() const { return {m_word_begin, m_word_end}; }

        Iterator& operator++()
        {
            const auto* end = m_splitter->m_content.end();
            auto extra_chars = m_splitter->m_extra_word_chars;

            do
            {
                auto it = m_word_begin = m_word_end;
                while (it != end and not is_word(utf8::read_codepoint(it, end), extra_chars))
                    m_word_begin = it;

                m_word_end = it;
                while (it != end and is_word(utf8::read_codepoint(it, end), extra_chars))
                    m_word_end = it;
            } while (m_word_begin != end and (m_word_end - m_word_begin) > max_word_len);

            return *this;
        }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs) = default;

        const char* m_word_begin;
        const char* m_word_end;
        const WordSplitter* m_splitter;
    };

    StringView m_content;
    ConstArrayView<Codepoint> m_extra_word_chars;

    Iterator begin() const { return {m_content.begin(), *this}; }
    Iterator end()   const { return {m_content.end(), *this}; }
};

}

#endif // word_splitter_hh_INCLUDED
