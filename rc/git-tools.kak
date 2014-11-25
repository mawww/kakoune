decl str docsclient

hook global WinSetOption filetype=git-log %{
    addhl group git-log-highlight
    addhl -group git-log-highlight regex '^(commit) ([0-9a-f]+)$' 1:yellow 2:red
    addhl -group git-log-highlight regex '^([a-zA-Z_-]+:) (.*?)$' 1:green 2:magenta
}

hook global WinSetOption filetype=(?!git-log).* %{
    rmhl git-log-highlight
}

hook global WinSetOption filetype=git-status %{
    addhl group git-status-highlight
    addhl -group git-status-highlight regex '^#\h+((modified:)|(added:)|(deleted:)|(renamed:)|(copied:))(.*?)$' 2:yellow 3:green 4:red 5:cyan 6:blue 7:magenta
}

hook global WinSetOption filetype=(?!git-status).* %{
    rmhl git-status-highlight
}

decl line-flag-list git_blame_flags
decl line-flag-list git_diff_flags

def -shell-params \
  -docstring %sh{printf "%%{git wrapping helper\navailable commands:\n add\n blame\n checkout\n diff\n hide-blame\n log\n show\n show-diff\n status\n update-diff}"} \
  -shell-completion %{
    shift $(expr ${kak_token_to_complete})
    prefix=${1:0:${kak_pos_in_token}}
    (
      for cmd in add blame checkout diff hide-blame log show show-diff status update-diff; do
          expr "${cmd}" : "^\(${prefix}.*\)$"
      done
    ) | grep -v '^$'
  } \
  git %{ %sh{
    show_git_cmd_output() {
        local filetype
        case "$1" in
           show|diff) filetype=diff ;;
           log)  filetype=git-log ;;
           status)  filetype=git-status ;;
        esac
        output=$(mktemp -d -t kak-git.XXXXXXXX)/fifo
        mkfifo ${output}
        ( git "$@" > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &

        echo "eval -try-client '$kak_opt_docsclient' %{
                  edit! -fifo ${output} *git*
                  set buffer filetype '${filetype}'
                  hook buffer BufCloseFifo .* %{ nop %sh{ rm -r $(dirname ${output}) } }
              }"
    }

    run_git_blame() {
        (
            echo "eval -client '$kak_client' %{
                      try %{ addhl flag_lines magenta git_blame_flags }
                      set buffer=$kak_bufname git_blame_flags ''
                  }" | kak -p ${kak_session}
                  git blame "$@" --incremental ${kak_buffile} | awk -e '
                  function send_flags(text, flag, i) {
                      if (line == "") { return; }
                      text=substr(sha,1,8) " " dates[sha] " " authors[sha]
                      gsub(":", "\\:", text)
                      gsub(",", "\\,", text)
                      flag=line ",black," text
                      for ( i=1; i < count; i++ ) {
                          flag=flag ":" line+i ",black," text
                      }
                      cmd = "kak -p " ENVIRON["kak_session"]
                      print "set -add buffer=" ENVIRON["kak_bufname"] " git_blame_flags %{" flag "}" | cmd
                      close(cmd)
                  }
                  /^([0-9a-f]{40}) ([0-9]+) ([0-9]+) ([0-9]+)/ {
                      send_flags()
                      sha=$1
                      line=$3
                      count=$4
                  }
                  /^author / { authors[sha]=substr($0,8) }
                  /^author-time ([0-9]*)/ {
                       cmd = "date -d @" $2 " +\"%F %T\""
                       cmd | getline dates[sha]
                  }
                  END { send_flags(); }'
        ) > /dev/null 2>&1 < /dev/null &
    }

    update_diff() {
        git diff -U0 $kak_buffile | awk -e '
            BEGIN {
                line=0
                flags="0,red,."
            }
            /^---.*/ {}
            /^@@ -[0-9]+(,[0-9]+)? \+[0-9]+(,[0-9]+)? @@.*/ {
                 if ((x=index($3, ",")) > 0) {
                     line=substr($3, 2, x-1)
                 } else {
                     line=substr($3, 2)
                 }
            }
            /^\+/ {
                 flags=flags ":" line ",green,+"
                 line++
            }
            /^\-/ { flags=flags ":" line ",red,-" }
            END { print "set buffer git_diff_flags ", flags }
        '
    }

    case "$1" in
       show|log|diff|status) show_git_cmd_output "$@" ;;
       blame) shift; run_git_blame "$@" ;;
       hide-blame) echo "try %{ rmhl hlflags_git_blame_flags }" ;;
       show-diff)
           echo "try %{ addhl flag_lines black git_diff_flags }"
           update_diff
           ;;
       update-diff) update_diff ;;
       checkout)
           name="${2:-${kak_buffile}}"
           git checkout "${name}"
           ;;
       add)
           name="${2:-${kak_buffile}}"
           if git add -- "${name}"; then
              echo "echo -color Information 'git: added ${name}'"
           else
              echo "echo -color Error 'git: unable to add ${name}'"
           fi
           ;;
       *) echo "echo -color Error %{unknown git command '$1'}"; exit ;;
    esac
}}
