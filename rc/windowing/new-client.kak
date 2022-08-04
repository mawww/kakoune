define-command new -params .. -docstring '
new [<commands>]: create a new Kakoune client
The ''terminal'' alias is being used to determine the user''s preferred terminal emulator
The optional arguments are passed as commands to the new client' \
%{
    terminal kak -c %val{session} -e "%arg{@}"
}

complete-command -menu new command
