define-command new -params .. -command-completion -docstring '
new [<commands>]: create a new kakoune client
The ''terminal'' alias is being used to determine the user''s preferred terminal emulator
The optional arguments are passed as commands to the new client' \
%{
    try %{
        terminal kak -c %val{session} -e "%arg{@}"
    } catch %{
        fail "The 'terminal' alias must be defined to use this command"
    }
}

