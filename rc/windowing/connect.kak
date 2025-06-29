define-command -override connect -params 1.. -command-completion -docstring 'Run a command as <command> sh -c {connect} -- [arguments].  Example: connect terminal sh' %{
  %arg{1} sh -c %{
    export KAKOUNE_SESSION=$1
    export KAKOUNE_CLIENT=$2
    shift 3

    export EDITOR='kak-connect'

    [ $# = 0 ] && set -- "$SHELL"

    "$@"
  } -- %val{session} %val{client} %arg{@}
}

define-command -override run -params 1.. -shell-completion -docstring 'Run a program in a new session' %{
  nop %sh{
    nohup "$@" < /dev/null > /dev/null 2>&1 &
  }
}
