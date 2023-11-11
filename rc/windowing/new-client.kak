define-command new -params .. -docstring '
new [<commands>]: create a new Kakoune client
The ''terminal'' alias is being used to determine the user''s preferred terminal emulator
The optional arguments are passed as commands to the new client' \
%{
    terminal kak -c %val{session} -e %exp{
        evaluate-commands -save-regs ^ %%{
            try %%{
                execute-keys -draft -client %val{client} -save-regs '' Z
                execute-keys z
                echo # clear message from z
            }
        }
        %sh{
            for arg
            do
                printf %s "'$(printf %s "$arg" | sed "s/'/''/g")' "
            done
        }
    }
}

complete-command -menu new command
