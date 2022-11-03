#include "register_manager.hh"

#include "assert.hh"
#include "context.hh"
#include "hash_map.hh"
#include "string_utils.hh"

namespace Kakoune
{

void StaticRegister::set(Context& context, ConstArrayView<String> values, bool)
{
    m_content.assign(values.begin(), values.end());
    if (not m_disable_modified_hook)
        context.hooks().run_hook(Hook::RegisterModified, m_name, context);
}

ConstArrayView<String> StaticRegister::get(const Context&)
{
    if (m_content.empty())
        return ConstArrayView<String>(String::ms_empty);
    else
        return ConstArrayView<String>(m_content);
}

const String& StaticRegister::get_main(const Context& context, size_t main_index)
{
    auto content = get(context);
    return content[std::min(main_index, content.size() - 1)];
}

void HistoryRegister::set(Context& context, ConstArrayView<String> values, bool restoring)
{
    constexpr size_t size_limit = 100;

    if (restoring)
    {
        StaticRegister::set(context, values, true);
        m_content.erase(remove_if(m_content, [](auto&& s) { return s.empty(); }), m_content.end());
        return;
    }

    for (auto&& entry : values | reverse()) 
    {
        m_content.erase(std::remove(m_content.begin(), m_content.end(), entry), m_content.end());
        m_content.insert(m_content.begin(), entry);
    }

    if (m_content.size() > size_limit)
        m_content.erase(m_content.end() - (m_content.size() - size_limit), m_content.end());

    if (not m_disable_modified_hook)
        context.hooks().run_hook(Hook::RegisterModified, m_name, context);
}

const String& HistoryRegister::get_main(const Context&, size_t)
{
    return m_content.empty() ? String::ms_empty : m_content.front();
}

static const HashMap<StringView, Codepoint> reg_names {
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
    return complete(prefix, cursor_pos, reg_names | transform([](auto& i) { return i.key; }));
}

}
