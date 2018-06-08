# ghcid integration (loosely based on make.kak)
#
# Starts ghcid in the background. all commands are executed on the
# client specified by the 'ghcidtoolsclient' option.
#
# Recommended way to use this is to define a command in your kakrc
# to split off a new client for showing the ghcid output:
#
#   set-option global ghcidtoolsclient ghcid
#   define-command ghcid %{
#     iterm-new-vertical "rename-client ghcid; ghcid-start"
#   %}
#
# And you may want to bind the next and previous error commands to nice keys,
# e.g. here ",p", and ",n":
#   map -docstring "jump to previous error" global user p ':ghcid-previous-error<ret>'
#   map -docstring "jump to next error" global user n ':ghcid-next-error<ret>'

# options

declare-option -docstring "name of the client in which to jump" \
  str ghcidclient

declare-option -docstring "name of the client in which to process output" \
  str ghcidtoolsclient

declare-option -docstring "ghcid error regexp" \
  str ghcid_error_pattern "[^:\n]+:\d+:\d+: error:$"

declare-option -hidden int ghcid_current_error_line

# commands

define-command -params .. \
  -docstring %{ghcid-start [<arguments>]: ghcid utility wrapper
All optional arguments forwarded to ghcid} \
  ghcid-start %{ %sh{
    workdir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-ghcid.XXXXXXXXX)
    output=${workdir}/output
    errors=${workdir}/errors
    fifo=${workdir}/fifo
    exec 2> ${errors}
    touch ${output}
    mkfifo ${fifo}
    cmd="ghcid -S -n -o ${output} $@"
    echo "starting: ${cmd}" > ${output}
    $cmd > /dev/null < /dev/null &
    pid=$!
    (tail -qf ${errors} ${output} > ${fifo} 2>&1) > /dev/null 2>&1 < /dev/null &
    pid2=$!

    printf %s\\n "evaluate-commands -try-client '$kak_opt_ghcidtoolsclient' %{
      edit! -fifo ${fifo} -scroll *ghcid*
      set-option buffer filetype ghcid
      set-option buffer ghcid_current_error_line 0
      hook -group ghcid buffer BufCloseFifo .* %{
        nop %sh{ kill ${pid} ${pid2}; rm -r ${output} }
        remove-hooks buffer ghcid
        remove-hooks global ghcid
      }
      # hook to writes to .(l)hs files and clear the buffer
      hook -group ghcid global BufWritePost .*\.l?hs %{
        evaluate-commands -try-client %opt{ghcidtoolsclient} %{
          buffer '*ghcid*'
          execute-keys '%d'
          set-option buffer ghcid_current_error_line 0
        }
      }
    }"
  }
}

define-command -hidden ghcid-jump %{
    evaluate-commands %{
        try %{
            set-option buffer ghcid_current_error_line %val{cursor_line}
            # find the whole error block
            execute-keys "gh?" "[^\n]+\n((\s|\d)+[^\n]+\n)*" <ret>
            # parse out the filepath, the first error line and the rest of the error
            execute-keys s "([^:\n]+):(\d+):(\d+): error:\n([^\n]+)\n((\s|\d)+[^\n]+\n)*" <ret>l
            # switch to the file and show the error
            evaluate-commands -try-client %opt{ghcidclient} "edit -existing %reg{1} %reg{2} %reg{3}; echo -markup {Information}%reg{4}; try %{ focus }"
        }
    }
}

define-command ghcid-next-error -docstring 'Jump to next ghcid error' %{
    evaluate-commands -try-client %opt{ghcidclient} %{
        try %{
          buffer '*ghcid*'
          execute-keys "%opt{ghcid_current_error_line}ggl" "?%opt{ghcid_error_pattern}<ret>"
          ghcid-jump
        } catch %{
          # jump back to previous buffer
          execute-keys "ga"
          echo -markup "{Information}No more errors"
        }
    }
    try %{ evaluate-commands -client %opt{ghcidtoolsclient} %{ execute-keys %opt{ghcid_current_error_line}g vc } }
}

define-command ghcid-previous-error -docstring 'Jump to previous ghcid error' %{
    evaluate-commands -try-client %opt{ghcidclient} %{
        buffer '*ghcid*'
        execute-keys "%opt{ghcid_current_error_line}g" "<a-/>%opt{ghcid_error_pattern}<ret>"
        ghcid-jump
    }
    try %{ evaluate-commands -client %opt{ghcidtoolsclient} %{ execute-keys %opt{ghcid_current_error_line}g vc } }
}

# highlighting

add-highlighter shared/ group ghcid
# highlight the "file:row:col error:" line
add-highlighter shared/ghcid regex "^((?:\w:)?[^:\n]+):(\d+):(?:(\d+):)?\h+(?:(error)|(warning))?.*?$" 1:cyan 2:green 3:green 4:red 5:yellow
# make the current error line underlined and bold
add-highlighter shared/ghcid line '%opt{ghcid_current_error_line}' default+bu

hook -group ghcid-highlight global WinSetOption filetype=ghcid %{ add-highlighter window ref ghcid }
hook -group ghcid-highlight global WinSetOption filetype=(?!ghcid).* %{ remove-highlighter window/ghcid }

hook global WinSetOption filetype=ghcid %{
  hook buffer -group ghcid-hooks NormalKey <ret> ghcid-jump
}

hook global WinSetOption filetype=(?!ghcid).* %{
    remove-hooks buffer ghcid-hooks
}

