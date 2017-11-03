# http://ranger.nongnu.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

define-command ranger-open-on-edit-directory \
    -docstring 'Start the ranger file system explorer when trying to edit a directory' %{
        hook global RuntimeError "\d+:\d+: '\w+' (.*): is a directory" %{ %sh{
          directory=$kak_hook_param_capture_1
          echo ranger $directory
    }}
}

define-command \
  -params .. -file-completion \
  -docstring %{ranger [<arguments>]: open the file system explorer to select buffers to open
All the optional arguments are forwarded to the ranger utility} \
  ranger %{ %sh{
  if [ -n "$TMUX" ]; then
    tmux split-window -h \
      ranger $@ --cmd " \
        map <return> eval \
          fm.execute_console('shell \
            echo evaluate-commands -client $kak_client edit {file} | \
            kak -p $kak_session; \
            tmux select-pane -t $kak_client_env_TMUX_PANE'.format(file=fm.thisfile.path)) \
          if fm.thisfile.is_file else fm.execute_console('move right=1')"
  elif [ -n "$WINDOWID" ]; then
    setsid $kak_opt_termcmd " \
      ranger $@ --cmd "'"'" \
        map <return> eval \
          fm.execute_console('shell \
            echo evaluate-commands -client $kak_client edit {file} | \
            kak -p $kak_session; \
            xdotool windowactivate $kak_client_env_WINDOWID'.format(file=fm.thisfile.path)) \
          if fm.thisfile.is_file else fm.execute_console('move right=1')"'"' < /dev/null > /dev/null 2>&1 &
  fi
}}
