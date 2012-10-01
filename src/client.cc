#include "client.hh"

#include "context.hh"
#include "editor.hh"
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
    void reset_normal_mode();
private:
    Client& m_client;
};

namespace ClientModes
{

class Normal : public ClientMode
{
public:
    Normal(Client& client)
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
                context.numeric_param() = m_count;
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

class Menu : public ClientMode
{
public:
    Menu(Context& context, const memoryview<String>& choices,
         MenuCallback callback)
        : ClientMode(context.client()),
          m_callback(callback), m_choice_count(choices.size()), m_selected(0)
    {
        DisplayCoord menu_pos{ context.window().dimensions().line, 0_char };
        context.ui().menu_show(choices, menu_pos, MenuStyle::Prompt);
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
            context.ui().menu_select(m_selected);
        }
        if (key == Key::Up or
            key == Key::BackTab or
            key == Key(Key::Modifiers::Control, 'p') or
            key == Key(Key::Modifiers::None, 'k'))
        {
            if (--m_selected < 0)
                m_selected = m_choice_count-1;
            context.ui().menu_select(m_selected);
        }
        if (key == Key(Key::Modifiers::Control, 'm'))
        {
            context.ui().menu_hide();
            // save callback as reset_normal_mode will delete this
            MenuCallback callback = std::move(m_callback);
            int selected = m_selected;
            reset_normal_mode();
            callback(selected, context);
        }
        if (key == Key::Escape)
        {
            context.ui().menu_hide();
            reset_normal_mode();
        }
        if (key.modifiers == Key::Modifiers::None and
            key.key >= '0' and key.key <= '9')
        {
            context.ui().menu_hide();
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

class Prompt : public ClientMode
{
public:
    Prompt(Context& context, const String& prompt,
           Completer completer, PromptCallback callback)
        : ClientMode(context.client()), m_prompt(prompt),
          m_completer(completer), m_callback(callback)
    {
        m_history_it = ms_history[m_prompt].end();
        context.ui().print_status(m_prompt, m_prompt.length());
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
            context.ui().print_status("");
            context.ui().menu_hide();
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
            context.ui().print_status("");
            context.ui().menu_hide();
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

            context.ui().menu_hide();
            m_current_completion = -1;
        }
        else if (key == Key(Key::Modifiers::Control, 'r'))
        {
            Key k = context.ui().get_key();
            String reg = RegisterManager::instance()[k.key].values(context)[0];
            context.ui().menu_hide();
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

                context.ui().menu_hide();
                DisplayCoord menu_pos{ context.window().dimensions().line, 0_char };
                context.ui().menu_show(candidates, menu_pos, MenuStyle::Prompt);
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
            context.ui().menu_select(m_current_completion);
            m_result = m_result.substr(0, m_completions.start) + completion
                     + m_result.substr(m_cursor_pos);
            m_cursor_pos = m_completions.start + completion.length();
        }
        else
        {
            context.ui().menu_hide();
            m_current_completion = -1;
            m_result = m_result.substr(0, m_cursor_pos) + key.key + m_result.substr(m_cursor_pos, String::npos);
            ++m_cursor_pos;
        }
        context.ui().print_status(m_prompt + m_result, m_prompt.length() + m_cursor_pos);
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
std::unordered_map<String, std::vector<String>> Prompt::ms_history;

class NextKey : public ClientMode
{
public:
    NextKey(Client& client, KeyCallback callback)
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

class WordCompleter
{
public:
    void select(const Context& context, int offset)
    {
        if (not setup_ifn(context))
            return;

        context.buffer().erase(m_position, m_position + m_completions[m_current_completion].length());
        m_current_completion = (m_current_completion + offset) % m_completions.size();
        context.buffer().insert(m_position, m_completions[m_current_completion]);
        context.ui().menu_select(m_current_completion);
    }

    void reset(const Context& context)
    {
        m_position = BufferIterator();
        context.ui().menu_hide();
    }
private:
    bool setup_ifn(const Context& context)
    {
        if (not m_position.is_valid())
        {
            BufferIterator end = context.editor().selections().back().last();
            BufferIterator begin = end-1;
            while (not begin.is_begin() and is_word(*begin))
                --begin;
            if (not is_word(*begin))
                ++begin;

            String prefix = context.buffer().string(begin, end);
            m_completions = complete_word(context, prefix);
            if (m_completions.empty())
                return false;

            m_position = begin;
            DisplayCoord menu_pos = context.window().display_position(m_position);
            context.ui().menu_show(m_completions, menu_pos, MenuStyle::Inline);

            m_completions.push_back(prefix);
            m_current_completion = m_completions.size() - 1;
        }
        return true;
    }

    CandidateList complete_word(const Context& context,
                                const String& prefix)
    {
        String ex = "\\<\\Q" + prefix + "\\E\\w+\\>";
        Regex re(ex.begin(), ex.end());
        Buffer& buffer = context.buffer();
        boost::regex_iterator<BufferIterator> it(buffer.begin(), buffer.end(), re);
        boost::regex_iterator<BufferIterator> end;

        CandidateList result;
        while (it != end)
        {
            auto& match = (*it)[0];
            String content = buffer.string(match.first, match.second);
            if (not contains(result, content))
                result.emplace_back(std::move(content));
            ++it;
        }
        std::sort(result.begin(), result.end());
        return result;
    }


    BufferIterator m_position;
    CandidateList  m_completions;
    int            m_current_completion = -1;
};

class Insert : public ClientMode
{
public:
    Insert(Context& context, InsertMode mode)
        : ClientMode(context.client()),
          m_inserter(context.editor(), mode)
    {
        context.last_insert().first = mode;
        context.last_insert().second.clear();
    }

    void on_key(const Key& key, Context& context) override
    {
        context.last_insert().second.push_back(key);
        if (m_insert_reg)
        {
            if (key.modifiers == Key::Modifiers::None)
                m_inserter.insert(RegisterManager::instance()[key.key].values(context));
            m_insert_reg = false;
            return;
        }
        bool reset_completer = true;
        switch (key.modifiers)
        {
        case Key::Modifiers::None:
            switch (key.key)
            {
            case Key::Escape:
                m_completer.reset(context);
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
                if (m_inserter.editor().selections().size() == 1 and
                    is_word(key.key))
                {
                    m_completer.reset(context);
                    reset_completer = false;
                    m_completer.select(context, 0);
                }
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
            case 'n':
                m_completer.select(context, 1);
                reset_completer = false;
                break;
            case 'p':
                m_completer.select(context, -1);
                reset_completer = false;
                break;
            }
            break;
        }
        if (reset_completer)
            m_completer.reset(context);
    }
private:
    bool m_insert_reg = false;
    IncrementalInserter m_inserter;
    WordCompleter m_completer;
};

}

void ClientMode::reset_normal_mode()
{
     m_client.m_mode.reset(new ClientModes::Normal(m_client));
}


Client::Client()
    : m_mode(new ClientModes::Normal(*this))
{
}

Client::~Client()
{
}

void Client::insert(Context& context, InsertMode mode)
{
    assert(&context.client() == this);
    m_mode.reset(new ClientModes::Insert(context, mode));
}

void Client::repeat_last_insert(Context& context)
{
    assert(&context.client() == this);
    Context::Insertion& last_insert = context.last_insert();
    if (last_insert.second.empty())
        return;

    std::vector<Key> keys;
    swap(keys, last_insert.second);
    // context.last_insert will be refilled by the new Insert
    // this is very inefficient.
    m_mode.reset(new ClientModes::Insert(context, last_insert.first));
    for (auto& key : keys)
        m_mode->on_key(key, context);
    assert(dynamic_cast<ClientModes::Normal*>(m_mode.get()) != nullptr);
}

void Client::prompt(const String& prompt, Completer completer,
                    PromptCallback callback, Context& context)
{
    assert(&context.client() == this);
    m_mode.reset(new ClientModes::Prompt(context, prompt, completer, callback));
}

void Client::menu(const memoryview<String>& choices,
                  MenuCallback callback, Context& context)
{
    assert(&context.client() == this);
    m_mode.reset(new ClientModes::Menu(context, choices, callback));
}

void Client::on_next_key(KeyCallback callback)
{
    m_mode.reset(new ClientModes::NextKey(*this, callback));
}

void Client::handle_next_input(Context& context)
{
    m_mode->on_key(context.ui().get_key(), context);
    context.draw_ifn();
}

}
