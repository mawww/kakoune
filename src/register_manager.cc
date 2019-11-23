#include "register_manager.hh"

#include "assert.hh"
#include "hash_map.hh"
#include "string_utils.hh"

namespace Kakoune
{

static const HashMap<String, Codepoint> reg_names = {
    { "slash", '/' },
    { "dquote", '"' },
    { "pipe", '|' },
    { "caret", '^' },
    { "arobase", '@' },
    { "percent", '%' },
    { "dot", '.' },
    { "hash", '#' },
    { "underscore", '_' },
    { "colon", ':' }
};

Register& RegisterManager::operator[](StringView reg) const
{
    if (reg.length() == 1)
        return (*this)[reg[0_byte]];

    auto it = reg_names.find(reg);
    if (it == reg_names.end())
        throw runtime_error(format("no such register: '{}'", reg));
    return (*this)[it->value];
}

Register& RegisterManager::operator[](Codepoint c) const
{
    c = to_lower(c);
    auto it = m_registers.find(c);
    if (it == m_registers.end())
        throw runtime_error(format("no such register: '{}'", c));

    return *(it->value);
}

void RegisterManager::add_register(Codepoint c, std::unique_ptr<Register> reg)
{
    auto& reg_ptr = m_registers[c];
    kak_assert(not reg_ptr);
    reg_ptr = std::move(reg);
}

CandidateList RegisterManager::complete_register_name(StringView prefix, ByteCount cursor_pos) const
{
    return complete(prefix, cursor_pos, reg_names | transform([](auto& i) { return i.key; }) | gather<Vector<String>>());
}

}
