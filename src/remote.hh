#ifndef remote_hh_INCLUDED
#define remote_hh_INCLUDED

#include "user_interface.hh"
#include "display_buffer.hh"

namespace Kakoune
{

struct peer_disconnected {};

class RemoteUI : public UserInterface
{
public:
    RemoteUI(int socket);
    ~RemoteUI();

    void print_status(const String& status, CharCount cursor_pos) override;

    void menu_show(const memoryview<String>& choices,
                   const DisplayCoord& anchor, MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(const String& content,
                   const DisplayCoord& anchor, MenuStyle style) override;
    void info_hide() override;

    void draw(const DisplayBuffer& display_buffer,
              const String& mode_line) override;

    bool is_key_available() override;
    Key  get_key() override;
    DisplayCoord dimensions() override;

private:
    int          m_socket;
    DisplayCoord m_dimensions;
};

class RemoteClient
{
public:
    RemoteClient(int socket, UserInterface* ui);

    void process_next_message();
    void write_next_key();

private:
    int                            m_socket;
    std::unique_ptr<UserInterface> m_ui;
    DisplayCoord                   m_dimensions;
};

}

#endif // remote_hh_INCLUDED

