define-command new -params .. -docstring '
new [<commands>]: create a new Kakoune client
The ''terminal'' alias is being used to determine the user''s preferred terminal emulator
The optional arguments are passed as commands to the new client' \
%{

    evaluate-commands %sh{
        if [ $# -gt 0 ]; then
            echo "echo -debug '$@'"
            echo "terminal kak -c %val{session} -e \"$@\""
        else
            echo "terminal kak -c %val{session}"
        fi
    }
}

complete-command -menu new command
