
provide-module explore-files %{

require-module connect

# explorefilescmd should be set such as the next argument is file or
# directory to open
declare-option -docstring %{command run to spawn a file browser} \
    str explorefilescmd %sh{
    for explorefilescmd in 'terminal nnn' \
                   'terminal ranger'; do
        cmd=${explorefilescmd##* }
        if command -v $cmd >/dev/null 2>&1; then
            printf %s\\n "$explorefilescmd"
            exit
        fi
    done
}

define-command -override -params ..1 explore-files %{
    evaluate-commands %sh{
        if [ -z "$kak_opt_explorefilescmd" ]; then
            echo "fail 'explorefilescmd option is not set'"
            exit
        fi
        printf 'connect %s "%s"\n' "$kak_opt_explorefilescmd" "$1"
    }
}
complete-command explore-files file

hook global RuntimeError "\d+:\d+: '(edit|e)':? wrong argument count" %{
  evaluate-commands -save-regs 'd' %{
    set-register d %sh(dirname "$kak_buffile")
    explore-files %reg{d}
    echo "Openned file browser !"
  }
}

hook global RuntimeError "\d+:\d+: '(?:edit|e)':? (.+): is a directory" %{
  explore-files %val{hook_param_capture_1}
  echo "Openned file browser !"
}

hook global RuntimeError "unable to find file '(.+)'" %{
  explore-files %val{hook_param_capture_1}
  echo "Openned file browser !"
}

hook global KakBegin .* %{
  hook -once global ClientCreate .* %{
    try %{
      evaluate-commands -buffer '*debug*' -save-regs '/' %{
        set-register / "^error while opening file '(.+?)':?\n[^\n]+: is a directory$"
        execute-keys '%1s<ret>'
        evaluate-commands -draft -itersel -save-regs 'd' %{
          set-register d %reg{.}
          evaluate-commands -client %val{hook_param} %{
            explore-files %reg{d}
            echo "Openned file browser !"
          }
        }
      }
    }
  }
}

}
