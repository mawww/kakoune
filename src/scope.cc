#include "scope.hh"
#include "context.hh"

namespace Kakoune
{

GlobalScope::GlobalScope()
    : m_option_registry(m_options)
{
    options().register_watcher(*this);
}

GlobalScope::~GlobalScope()
{
    options().unregister_watcher(*this);
}

void GlobalScope::on_option_changed(const Option& option)
{
    Context empty_context{Context::EmptyContextFlag{}};
    hooks().run_hook(Hook::GlobalSetOption,
                     format("{}={}", option.name(), option.get_desc_string()),
                     empty_context);
}

}
