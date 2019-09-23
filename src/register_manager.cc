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
        return StaticRegister::set(context, values, true);

    for (auto& entry : values)
    {
        m_content.erase(std::remove(m_content.begin(), m_content.end(), entry),
                      m_content.end());
        m_content.push_back(entry);
    }

    const size_t current_size = m_content.size();
    if (current_size > size_limit)
        m_content.erase(m_content.begin(), m_content.begin() + (current_size - size_limit));

    if (not m_disable_modified_hook)
        context.hooks().run_hook(Hook::RegisterModified, m_name, context);
}

const String& HistoryRegister::get_main(const Context&, size_t)
{
    return last_entry();
}

ConstArrayView<String> HistoryRegister::get_for_pasting(Context&)
{
    return last_entry();
}

const String& HistoryRegister::last_entry() const
{
    return m_content.empty() ? String::ms_empty : m_content.back();
}

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
