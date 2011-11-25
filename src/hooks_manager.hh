#ifndef hooks_manager_hh_INCLUDED
#define hooks_manager_hh_INCLUDED

#include "window.hh"
#include "utils.hh"

#include <unordered_map>

namespace Kakoune
{

struct HookContext
{
    Window* window;
    Buffer* buffer;
    std::string context;

    HookContext(const std::string& context)
        : window(nullptr), buffer(nullptr), context(context) {}
    HookContext(const std::string& context, Window& window)
        : window(&window), buffer(&window.buffer()), context(context) {}
    HookContext(const std::string& context, Buffer& buffer)
        : window(nullptr), buffer(&buffer), context(context) {}
};

typedef std::function<void (const HookContext&)> HookFunc;

class HooksManager : public Singleton<HooksManager>
{
public:
    void add_hook(const std::string& hook_name, HookFunc hook);
    void run_hook(const std::string& hook_name,
                  const HookContext& context) const;

private:
    std::unordered_map<std::string, std::vector<HookFunc>> m_hooks;
};

}

#endif // hooks_manager_hh_INCLUDED

