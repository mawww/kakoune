decl str docsclient

hook global WinSetOption filetype=git-log %{
    addhl group git-log-highlight
    addhl -group git-log-highlight regex '^(commit) ([0-9a-f]+)$' 1:yellow 2:red
    addhl -group git-log-highlight regex '^([a-zA-Z_-]+:) (.*?)$' 1:green 2:magenta
}

hook global WinSetOption filetype=(?!git-log).* %{
    rmhl git-log-highlight
}

decl line-flag-list git_blame_flags
decl line-flag-list git_diff_flags

def -shell-params git %{ %sh{
    show_git_cmd_output() {
        local filetype
        case "$1" in
           show|diff) filetype=diff ;;
           log)  filetype=git-log ;;
        esac
        tmpfile=$(mktemp /tmp/kak-git-XXXXXX)
        if git "$@" > ${tmpfile}; then
            [ -n "$kak_opt_docsclient" ] && echo "eval -client '$kak_opt_docsclient' %{"

            echo "edit! -scratch *git*
                  exec |cat<space>${tmpfile}<ret>gk
                  nop %sh{rm ${tmpfile}}
                  set buffer filetype '${filetype}'"

            [ -n "$kak_opt_docsclient" ] && echo "}"
        else
           echo "echo %{git $@ failed, see *debug* buffer}"
           rm ${tmpfile}
        fi
    }

    run_git_blame() {
        (
            echo "eval -client '$kak_client' %{
                      try %{ addhl flag_lines magenta git_blame_flags }
                      set buffer=$kak_bufname git_blame_flags ''
                  }" | kak -p ${kak_session}
                  git blame --incremental ${kak_buffile} | awk -e '
                  function send_flags(text, flag, i) {
                      if (line == "") { return; }
                      text=substr(sha,1,8) " " dates[sha] " " authors[sha]
                      gsub(":", "\\:", text)
                      flag=line "|black|" text
                      for ( i=1; i < count; i++ ) {
                          flag=flag ":" line+i "|black|" text
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
                flags="0|red|."
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
                 flags=flags ":" line "|green|+"
                 line++
            }
            /^\-/ { flags=flags ":" line "|red|-" }
            END { print "set buffer git_diff_flags ", flags }
        '
    }

    case "$1" in
       show|log|diff) show_git_cmd_output "$@" ;;
       blame) run_git_blame ;;
       show-diff)
           echo "try %{ addhl flag_lines black git_diff_flags }"
           update_diff
           ;;
       update-diff) update_diff ;;
       add)
           name="${2:-${kak_buffile}}"
           if git add -- "${name}"; then
              echo "echo -color Information 'git: added ${name}'"
           else
              echo "echo -color Error 'git: unable to add ${name}'"
           fi
           ;;
       *) echo "echo %{unknown git command '$1'}"; exit ;;
    esac

}}
