define-command new -params .. -command-completion -docstring %{
    new [<commands>]: spawn a new Kakoune client
    The new window's method of creation and its location are determined by the underlying `terminal` alias, called by this command
    The optional arguments are passed as commands to the new client
} %{
    terminal kak -c %val{session} -e "%arg{@}"
}

define-command new-window -params .. -command-completion -docstring %{
    new-window [<commands>]: spawn a Kakoune client in a new window
    The new window's method of creation and its location are determined by the underlying `terminal` alias, called by this command
    The optional arguments are passed as commands to the new client
} %{
    terminal-window kak -c %val{session} -e "%arg{@}"
}

define-command new-tab -params .. -command-completion -docstring %{
    new-tab [<commands>]: spawn a Kakoune client in a new tab
    The new tab's method of creation and its location are determined by the underlying `terminal` alias, called by this command
    The optional arguments are passed as commands to the new client
} %{
    terminal-tab kak -c %val{session} -e "%arg{@}"
}

define-command new-horizontal -params .. -command-completion -docstring %{
    new-horizontal [<commands>]: spawn a Kakoune client in a new window on the left/right of the current one
    The new window's method of creation and its location (left/right) are determined by the underlying `terminal` alias, called by this command
    The optional arguments are passed as commands to the new client
} %{
    terminal-horizontal kak -c %val{session} -e "%arg{@}"
}

define-command new-vertical -params .. -command-completion -docstring %{
    new-vertical [<commands>]: spawn a Kakoune client in a new window at the top/bottom of the current one
    The new window's method of creation and its location (up/down) are determined by the underlying `terminal` alias, called by this command
    The optional arguments are passed as commands to the new client
} %{
    terminal-vertical kak -c %val{session} -e "%arg{@}"
}
