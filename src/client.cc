#include "client.hh"

#include "context.hh"
#include "register_manager.hh"

#include <unordered_map>

namespace Kakoune
{

extern std::unordered_map<Key, std::function<void (Context& context)>> keymap;

class Client::NormalMode : public Client::Mode
{
public:
    NormalMode(Client& client)
        : Client::Mode(client)
    {
    }

    void on_key(const Key& key, Context& context) override
    {
        if (key.modifiers == Key::Modifiers::None and isdigit(key.key))
            m_count = m_count * 10 + key.key - '0';
        else
        {
            auto it = keymap.find(key);
            if (it != keymap.end())
            {
                context.numeric_param(m_count);
                // it's important to do that before calling the command,
                // as we may die during the command execution.
                m_count = 0;
                it->second(context);
            }
            else
                m_count = 0;
        }
    }

private:
    int m_count = 0;
};


class Client::MenuMode : public Client::Mode
{
public:
    MenuMode(Client& client, const memoryview<String>& choices, MenuCallback callback)
        : Client::Mode(client),
          m_callback(callback), m_choice_count(choices.size()), m_selected(0)
    {
        client.show_menu(choices);
    }

    ~MenuMode()
    {
        m_client.menu_ctrl(MenuCommand::Close);
    }

    void on_key(const Key& key, Context& context) override
    {
        if (key == Key(Key::Modifiers::Control, 'n') or
            key == Key(Key::Modifiers::Control, 'i') or
            key == Key(Key::Modifiers::None, 'j'))
        {
            if (++m_selected >= m_choice_count)
            {
                m_client.menu_ctrl(MenuCommand::SelectFirst);
                m_selected = 0;
            }
            else
                m_client.menu_ctrl(MenuCommand::SelectNext);
        }
        if (key == Key(Key::Modifiers::Control, 'p') or
            key == Key(Key::Modifiers::None, 'k'))
        {
            if (--m_selected < 0)
            {
                m_client.menu_ctrl(MenuCommand::SelectLast);
                m_selected = m_choice_count-1;
            }
            else
                m_client.menu_ctrl(MenuCommand::SelectPrev);
        }
        if (key == Key(Key::Modifiers::Control, 'm'))
        {
            // save callback as reset_normal_mode will delete this
            MenuCallback callback = std::move(m_callback);
            int selected = m_selected;
            m_client.reset_normal_mode();
            callback(selected, context);
        }
        if (key == Key(Key::Modifiers::None, 27))
        {
            m_client.reset_normal_mode();
        }
        if (key.modifiers == Key::Modifiers::None and
            key.key >= '0' and key.key <= '9')
        {
            m_client.menu_ctrl(MenuCommand::Close);
            // save callback as reset_normal_mode will delete this
            MenuCallback callback = std::move(m_callback);
            m_client.reset_normal_mode();
            callback(key.key - '0' - 1, context);
        }
   }

private:
    MenuCallback m_callback;
    int          m_selected;
    int          m_choice_count;
};

class Client::PromptMode : public Client::Mode
{
public:
    PromptMode(Client& client, const String& prompt, Completer completer, PromptCallback callback)
        : Client::Mode(client),
          m_prompt(prompt), m_completer(completer), m_callback(callback), m_cursor_pos(0)
    {
        m_history_it = ms_history[m_prompt].end();
        m_client.print_status(m_prompt, m_prompt.length());
    }

    ~PromptMode()
    {
        m_client.menu_ctrl(MenuCommand::Close);
    }

    void on_key(const Key& key, Context& context) override
    {
        std::vector<String>& history = ms_history[m_prompt];
        if (key == Key(Key::Modifiers::Control, 'm')) // enter
        {
            std::vector<String>::iterator it;
            while ((it = find(history, m_result)) != history.end())
                history.erase(it);

            history.push_back(m_result);
            m_client.print_status("");
            // save callback as reset_normal_mode will delete this
            PromptCallback callback = std::move(m_callback);
            String result = std::move(m_result);
            m_client.reset_normal_mode();
            callback(result, context);
            return;
        }
        else if (key == Key(Key::Modifiers::None, 27))
        {
            m_client.print_status("");
            m_client.reset_normal_mode();
            return;
        }
        else if (key == Key(Key::Modifiers::Control, 'p') or
                 key == Key(Key::Modifiers::Control, 'c'))
        {
            if (m_history_it != history.begin())
            {
                if (m_history_it == history.end())
                   m_saved_result = m_result;
                --m_history_it;
                m_result = *m_history_it;
                m_cursor_pos = m_result.length();
            }
        }
        else if (key == Key(Key::Modifiers::Control, 'n') or
                 key == Key(Key::Modifiers::Control, 'b'))
        {
            if (m_history_it != history.end())
            {
                ++m_history_it;
                if (m_history_it != history.end())
                    m_result = *m_history_it;
                else
                    m_result = m_saved_result;
                m_cursor_pos = m_result.length();
            }
        }
        else if (key == Key(Key::Modifiers::Control, 'd'))
        {
            if (m_cursor_pos > 0)
                --m_cursor_pos;
        }
        else if (key == Key(Key::Modifiers::Control, 'e'))
        {
            if (m_cursor_pos < m_result.length())
                ++m_cursor_pos;
        }
        else if (key == Key(Key::Modifiers::Control, 'g')) // backspace
        {
            if (m_cursor_pos != 0)
            {
                m_result = m_result.substr(0, m_cursor_pos - 1)
                       + m_result.substr(m_cursor_pos, String::npos);

                --m_cursor_pos;
            }

            m_client.menu_ctrl(MenuCommand::Close);
            m_current_completion = -1;
        }
        else if (key == Key(Key::Modifiers::Control, 'r'))
        {
            Key k = m_client.get_key();
            String reg = RegisterManager::instance()[k.key].values(context)[0];
            m_client.menu_ctrl(MenuCommand::Close);
            m_current_completion = -1;
            m_result = m_result.substr(0, m_cursor_pos) + reg + m_result.substr(m_cursor_pos, String::npos);
            m_cursor_pos += reg.length();
        }
        else if (key == Key(Key::Modifiers::Control, 'i')) // tab
        {
            if (m_current_completion == -1)
            {
                m_completions = m_completer(context, m_result, m_cursor_pos);
                if (m_completions.candidates.empty())
                    return;

                m_client.menu_ctrl(MenuCommand::Close);
                m_client.show_menu(m_completions.candidates);
                m_text_before_completion = m_result.substr(m_completions.start,
                                                         m_completions.end - m_completions.start);
            }
            else
                m_client.menu_ctrl(MenuCommand::SelectNext);
            ++m_current_completion;

            String completion;
            if (m_current_completion >= m_completions.candidates.size())
            {
                if (m_current_completion == m_completions.candidates.size() and
                    std::find(m_completions.candidates.begin(), m_completions.candidates.end(), m_text_before_completion) == m_completions.candidates.end())
                {
                    completion = m_text_before_completion;
                    m_client.menu_ctrl(MenuCommand::SelectNone);
                }
                else
                {
                    m_current_completion = 0;
                    completion = m_completions.candidates[0];
                    m_client.menu_ctrl(MenuCommand::SelectFirst);
                }
            }
            else
                completion = m_completions.candidates[m_current_completion];

            m_result = m_result.substr(0, m_completions.start) + completion;
            m_cursor_pos = m_completions.start + completion.length();
        }
        else
        {
            m_client.menu_ctrl(MenuCommand::Close);
            m_current_completion = -1;
            m_result = m_result.substr(0, m_cursor_pos) + key.key + m_result.substr(m_cursor_pos, String::npos);
            ++m_cursor_pos;
        }
        m_client.print_status(m_prompt + m_result, m_prompt.length() + m_cursor_pos);
    }

private:
    PromptCallback m_callback;
    Completer      m_completer;
    const String   m_prompt;
    CharCount      m_cursor_pos;
    Completions    m_completions;
    int            m_current_completion = -1;
    String         m_text_before_completion;
    String         m_result;
    String         m_saved_result;

    static std::unordered_map<String, std::vector<String>> ms_history;
    std::vector<String>::iterator m_history_it;
};
std::unordered_map<String, std::vector<String>> Client::PromptMode::ms_history;

Client::Client()
    : m_mode(new NormalMode(*this))
{
}

void Client::prompt(const String& prompt, Completer completer,
                    PromptCallback callback)
{
    m_mode.reset(new PromptMode(*this, prompt, completer, callback));
}

void Client::menu(const memoryview<String>& choices,
                  MenuCallback callback)
{
    m_mode.reset(new MenuMode(*this, choices, callback));
}

void Client::handle_next_input(Context& context)
{
    m_mode->on_key(get_key(), context);
    context.draw_ifn();
}

void Client::reset_normal_mode()
{
    m_mode.reset(new NormalMode(*this));
}

}
