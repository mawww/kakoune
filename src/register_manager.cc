#include "register_manager.hh"

#include "assert.hh"
#include "context.hh"
#include "file.hh"
#include "hash_map.hh"
#include "session_manager.hh"
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

FileRegister::FileRegister(Session& session, StringView path)
    : m_path{path}
{
    make_directory(session.file(m_path), 0711);
}

void FileRegister::set(Context& context, ConstArrayView<String> values)
{
    MemoryRegister::set(context, values);
    for (size_t i = 0; i < values.size(); ++i)
        context.session().write(format("{}/{}", m_path, i), values[i]);
    for (size_t i = values.size(); context.session().unlink(format("{}/{}", m_path, i)); ++i)
        ;
}

ConstArrayView<String> FileRegister::get(const Context& context)
{
    m_content.clear();
    for (size_t i = 0;; ++i)
    {
        try
        {
            m_content.emplace_back(context.session().read(format("{}/{}", m_path, i)));
        }
        catch (const runtime_error&)
        {
            break;
        }
    }
    return MemoryRegister::get(context);
}

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

String RegisterManager::name(Codepoint c)
{
    for (auto& entry : reg_names)
        if (entry.value == c)
            return entry.key;
    return String{c};
}

void RegisterManager::add_register(Codepoint c, std::unique_ptr<Register> reg)
{
    auto& reg_ptr = m_registers[c];
    kak_assert(not reg_ptr);
    reg_ptr = std::move(reg);
}

}
