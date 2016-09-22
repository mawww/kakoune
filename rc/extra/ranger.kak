# http://ranger.nongnu.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# the file manager option is used to decide which one should be used when trying to edit a directory.
decl str file_manager

hook global KakBegin .* %{ %sh{
  if [ -z "$kak_opt_file_manager" ]; then
    echo set global file_manager ranger
  fi
}}

hook global RuntimeError "\d+:\d+: '\w+' (.*): is a directory" %{ %sh{
  directory=$(echo $kak_hook_param | pcregrep --only-matching=1 "\d+:\d+: '\w+' (.*): is a directory")
  if [ "$kak_opt_file_manager" = ranger ]; then
    echo ranger $directory
  fi
}}

def ranger -docstring 'ranger file manager' \
           -params ..                       \
           -file-completion %{ %sh{
  if [ -n "$TMUX" ]; then
    tmux split-window -h ranger $@ --cmd "map <return> eval fm.execute_console('shell echo eval -client $kak_client edit {file} | kak -p $kak_session; tmux select-pane -t $kak_client_env_TMUX_PANE'.format(file=fm.thisfile.path)) if fm.thisfile.is_file else fm.execute_console('move right=1')"
  elif [ -n "$WINDOWID" ]; then
    setsid $kak_opt_termcmd "ranger $@ --cmd "'"'"map <return> eval fm.execute_console('shell echo eval -client $kak_client edit {file} | kak -p $kak_session; xdotool windowactivate $kak_client_env_WINDOWID'.format(file=fm.thisfile.path)) if fm.thisfile.is_file else fm.execute_console('move right=1')"'"' < /dev/null > /dev/null 2>&1 &
  fi
}}
