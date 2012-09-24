#include "client.hh"

#include "context.hh"
#include "register_manager.hh"

#include <unordered_map>

namespace Kakoune
{

extern std::unordered_map<Key, std::function<void (Context& context)>> keymap;

class ClientMode
{
public:
    ClientMode(Client& client) : m_client(client) {}
    virtual ~ClientMode() {}
    ClientMode(const ClientMode&) = delete;
    ClientMode& operator=(const ClientMode&) = delete;

    virtual void on_key(const Key& key, Context& context) = 0;
protected:
    UserInterface& ui() { return *m_client.m_ui; }
    void reset_normal_mode();
    std::pair<IncrementalInserter::Mode, std::vector<Key>>& last_insert() { return m_client.m_last_insert; }
private:
    Client& m_client;
};

class NormalMode : public ClientMode
{
public:
    NormalMode(Client& client)
        : ClientMode(client)
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

void ClientMode::reset_normal_mode()
{
     m_client.m_mode.reset(new NormalMode(m_client));
}

class MenuMode : public ClientMode
{
public:
    MenuMode(Client& client, const memoryview<String>& choices, MenuCallback callback)
        : ClientMode(client),
          m_callback(callback), m_choice_count(choices.size()), m_selected(0)
    {
        ui().menu_show(choices);
    }

    ~MenuMode()
    {
        ui().menu_hide();
    }

    void on_key(const Key& key, Context& context) override
    {
        if (key == Key::Down or
            key == Key(Key::Modifiers::Control, 'i') or
            key == Key(Key::Modifiers::Control, 'n') or
            key == Key(Key::Modifiers::None, 'j'))
        {
            if (++m_selected >= m_choice_count)
                m_selected = 0;
            ui().menu_select(m_selected);
        }
        if (key == Key::Up or
            key == Key::BackTab or
            key == Key(Key::Modifiers::Control, 'p') or
            key == Key(Key::Modifiers::None, 'k'))
        {
            if (--m_selected < 0)
                m_selected = m_choice_count-1;
            ui().menu_select(m_selected);
        }
        if (key == Key(Key::Modifiers::Control, 'm'))
        {
            // save callback as reset_normal_mode will delete this
            MenuCallback callback = std::move(m_callback);
            int selected = m_selected;
            reset_normal_mode();
            callback(selected, context);
        }
        if (key == Key::Escape)
        {
            reset_normal_mode();
        }
        if (key.modifiers == Key::Modifiers::None and
            key.key >= '0' and key.key <= '9')
        {
            ui().menu_hide();
            // save callback as reset_normal_mode will delete this
            MenuCallback callback = std::move(m_callback);
            reset_normal_mode();
            callback(key.key - '0' - 1, context);
        }
   }

private:
    MenuCallback m_callback;
    int          m_selected;
    int          m_choice_count;
};

class PromptMode : public ClientMode
{
public:
    PromptMode(Client& client, const String& prompt,
               Completer completer, PromptCallback callback)
        : ClientMode(client), m_prompt(prompt),
          m_completer(completer), m_callback(callback)
    {
        m_history_it = ms_history[m_prompt].end();
        ui().print_status(m_prompt, m_prompt.length());
    }

    ~PromptMode()
    {
        ui().menu_hide();
    }

    void on_key(const Key& key, Context& context) override
    {
        std::vector<String>& history = ms_history[m_prompt];
        if (key == Key{Key::Modifiers::Control, 'm'}) // enter
        {
            std::vector<String>::iterator it;
            while ((it = find(history, m_result)) != history.end())
                history.erase(it);

            history.push_back(m_result);
            ui().print_status("");
            // save callback as reset_normal_mode will delete this
            PromptCallback callback = std::move(m_callback);
            String result = std::move(m_result);
            reset_normal_mode();
            // call callback after reset_normal_mode so that callback
            // may change the mode
            callback(result, context);
            return;
        }
        else if (key == Key::Escape)
        {
            ui().print_status("");
            reset_normal_mode();
            return;
        }
        else if (key == Key::Up or
                 key == Key{Key::Modifiers::Control, 'p'})
        {
            if (m_history_it != history.begin())
            {
                if (m_history_it == history.end())
                   m_saved_result = m_result;
                auto it = m_history_it;
                // search for the previous history entry matching typed prefix
                CharCount prefix_length = m_saved_result.length();
                do
                {
                    --it;
                    if (it->substr(0, prefix_length) == m_saved_result)
                    {
                        m_history_it = it;
                        m_result     = *it;
                        m_cursor_pos = m_result.length();
                        break;
                    }
                } while (it != history.begin());
            }
        }
        else if (key == Key::Down or // next
                 key == Key{Key::Modifiers::Control, 'n'})
        {
            if (m_history_it != history.end())
            {
                CharCount prefix_length = m_saved_result.length();
                // search for the next history entry matching typed prefix
                ++m_history_it;
                while (m_history_it != history.end() and
                       m_history_it->substr(0, prefix_length) != m_saved_result)
                    ++m_history_it;

                if (m_history_it != history.end())
                    m_result = *m_history_it;
                else
                    m_result = m_saved_result;
                m_cursor_pos = m_result.length();
            }
        }
        else if (key == Key::Left or
                 key == Key{Key::Modifiers::Control, 'b'})
        {
            if (m_cursor_pos > 0)
                --m_cursor_pos;
        }
        else if (key == Key::Right or
                 key == Key{Key::Modifiers::Control, 'f'})
        {
            if (m_cursor_pos < m_result.length())
                ++m_cursor_pos;
        }
        else if (key == Key::Backspace)
        {
            if (m_cursor_pos != 0)
            {
                m_result = m_result.substr(0, m_cursor_pos - 1)
                       + m_result.substr(m_cursor_pos, String::npos);

                --m_cursor_pos;
            }

            ui().menu_hide();
            m_current_completion = -1;
        }
        else if (key == Key(Key::Modifiers::Control, 'r'))
        {
            Key k = ui().get_key();
            String reg = RegisterManager::instance()[k.key].values(context)[0];
            ui().menu_hide();
            m_current_completion = -1;
            m_result = m_result.substr(0, m_cursor_pos) + reg
                     + m_result.substr(m_cursor_pos, String::npos);
            m_cursor_pos += reg.length();
        }
        else if (key == Key(Key::Modifiers::Control, 'i') or // tab completion
                 key == Key::BackTab)
        {
            const bool reverse = (key == Key::BackTab);
            CandidateList& candidates = m_completions.candidates;
            // first try, we need to ask our completer for completions
            if (m_current_completion == -1)
            {
                m_completions = m_completer(context, m_result, m_cursor_pos);
                if (candidates.empty())
                    return;

                ui().menu_hide();
                ui().menu_show(candidates);
                String prefix = m_result.substr(m_completions.start,
                                                m_completions.end - m_completions.start);
                if (not contains(candidates, prefix))
                    candidates.push_back(std::move(prefix));
            }

            if (not reverse and ++m_current_completion >= candidates.size())
                m_current_completion = 0;
            if (reverse and --m_current_completion < 0)
                m_current_completion = candidates.size()-1;

            const String& completion = candidates[m_current_completion];
            ui().menu_select(m_current_completion);
            m_result = m_result.substr(0, m_completions.start) + completion
                     + m_result.substr(m_cursor_pos);
            m_cursor_pos = m_completions.start + completion.length();
        }
        else
        {
            ui().menu_hide();
            m_current_completion = -1;
            m_result = m_result.substr(0, m_cursor_pos) + key.key + m_result.substr(m_cursor_pos, String::npos);
            ++m_cursor_pos;
        }
        ui().print_status(m_prompt + m_result, m_prompt.length() + m_cursor_pos);
    }

private:
    PromptCallback m_callback;
    Completer      m_completer;
    const String   m_prompt;
    CharCount      m_cursor_pos = 0;
    Completions    m_completions;
    int            m_current_completion = -1;
    String         m_result;
    String         m_saved_result;

    static std::unordered_map<String, std::vector<String>> ms_history;
    std::vector<String>::iterator m_history_it;
};
std::unordered_map<String, std::vector<String>> PromptMode::ms_history;

class NextKeyMode : public ClientMode
{
public:
    NextKeyMode(Client& client, KeyCallback callback)
        : ClientMode(client), m_callback(callback) {}

    void on_key(const Key& key, Context& context) override
    {
        // save callback as reset_normal_mode will delete this
        KeyCallback callback = std::move(m_callback);
        reset_normal_mode();
        callback(key, context);
   }

private:
    KeyCallback m_callback;
};

class InsertMode : public ClientMode
{
public:
    InsertMode(Client& client, Editor& editor, IncrementalInserter::Mode mode)
        : ClientMode(client), m_inserter(editor, mode)
    {
        last_insert().first = mode;
        last_insert().second.clear();
    }

    void on_key(const Key& key, Context& context) override
    {
        last_insert().second.push_back(key);
        if (m_insert_reg)
        {
            if (key.modifiers == Key::Modifiers::None)
                m_inserter.insert(RegisterManager::instance()[key.key].values(context));
            m_insert_reg = false;
            return;
        }
        switch (key.modifiers)
        {
        case Key::Modifiers::None:
            switch (key.key)
            {
            case Key::Escape:
                reset_normal_mode();
                return;
            case Key::Backspace:
                m_inserter.erase();
                break;
            case Key::Left:
                m_inserter.move_cursors({0, -1});
                break;
            case Key::Right:
                m_inserter.move_cursors({0,  1});
                break;
            case Key::Up:
                m_inserter.move_cursors({-1, 0});
                break;
            case Key::Down:
                m_inserter.move_cursors({ 1, 0});
                break;
            default:
                m_inserter.insert(String() + key.key);
            }
            break;
        case Key::Modifiers::Control:
            switch (key.key)
            {
            case 'r':
                m_insert_reg = true;
                break;
            case 'm':
                m_inserter.insert(String() + '\n');
                break;
            case 'i':
                m_inserter.insert(String() + '\t');
                break;
            }
            break;
        }
    }
private:
    bool m_insert_reg = false;
    IncrementalInserter m_inserter;
};

Client::Client(UserInterface* ui)
    : m_mode(new NormalMode(*this)),
      m_ui(ui),
      m_last_insert(IncrementalInserter::Mode::Insert, {})
{
}

Client::~Client()
{
}

void Client::insert(Editor& editor, IncrementalInserter::Mode mode)
{
    m_mode.reset(new InsertMode(*this, editor, mode));
}

void Client::repeat_last_insert(Editor& editor, Context& context)
{
    if (m_last_insert.second.empty())
        return;

    std::vector<Key> keys;
    swap(keys, m_last_insert.second);
    // m_last_insert will be refilled by the new InsertMode
    // this is very inefficient.
    m_mode.reset(new InsertMode(*this, editor, m_last_insert.first));
    for (auto& key : keys)
        m_mode->on_key(key, context);
    assert(dynamic_cast<NormalMode*>(m_mode.get()) != nullptr);
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

void Client::on_next_key(KeyCallback callback)
{
    m_mode.reset(new NextKeyMode(*this, callback));
}

void Client::handle_next_input(Context& context)
{
    m_mode->on_key(m_ui->get_key(), context);
    context.draw_ifn();
}

void Client::print_status(const String& status, CharCount cursor_pos)
{
    m_ui->print_status(status, cursor_pos);
}

void Client::draw_window(Window& window)
{
    m_ui->draw_window(window);
}

}
