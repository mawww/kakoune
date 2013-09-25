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
            [[ -n "$kak_opt_docsclient" ]] && echo "eval -client '$kak_opt_docsclient' %{"

            echo "edit! -scratch *git*
                  exec |cat<space>${tmpfile}<ret>gk
                  nop %sh{rm ${tmpfile}}
                  setb filetype '${filetype}'"

            [[ -n "$kak_opt_docsclient" ]] && echo "}"
        else
           echo "echo %{git $@ failed, see *debug* buffer}"
           rm ${tmpfile}
        fi
    }

    run_git_blame() {
        (
            echo "eval -client '$kak_client' %{
                      try %{ addhl flag_lines magenta git_blame_flags } catch %{}
                      setb -buffer '$kak_bufname' git_blame_flags ''
                  }" | socat -u stdin UNIX-CONNECT:/tmp/kak-${kak_session}
            declare -A authors
            declare -A dates
            send_flags() {
                if [[ -z "$line" ]]; then return; fi
                text=$(echo "${sha:0:8} ${dates[$sha]} ${authors[$sha]}" | sed -e 's/:/\\:/g')
                flag="$line|black|$text"
                for (( i=1; $i < $count; i++ )); do
                    flag="$flag:$(($line+$i))|black|$text"
                done
                echo "setb -add -buffer '$kak_bufname' git_blame_flags %{${flag}}" | socat -u stdin UNIX-CONNECT:/tmp/kak-${kak_session}
            }
            git blame --incremental $kak_bufname | ( while read blame_line; do
                if [[ $blame_line =~ ([0-9a-f]{40}).([0-9]+).([0-9]+).([0-9]+) ]]; then
                    send_flags
                    sha=${BASH_REMATCH[1]}
                    line=${BASH_REMATCH[3]}
                    count=${BASH_REMATCH[4]}
                elif [[ $blame_line =~ author[^-](.*) ]]; then
                    authors[$sha]=${BASH_REMATCH[1]}
                elif [[ $blame_line =~ author-time.([0-9]*) ]]; then
                    dates[$sha]="$(date -d @${BASH_REMATCH[1]} +'%F %T')"
                fi
            done; send_flags )
        ) >& /dev/null < /dev/null &
    }

    update_diff() {
        git diff -U0 $kak_bufname | {
            local line=0
            local flags="0|red|."
            while read; do
                if [[ $REPLY =~ ^---.* ]]; then
                    continue
                elif [[ $REPLY =~ ^@@.-[0-9]+(,[0-9]+)?.\+([0-9]+)(,[0-9]+)?.@@.* ]]; then
                    line=${BASH_REMATCH[2]}
                elif [[ $REPLY =~ ^\+ ]]; then
                    flags="$flags:$line|green|+"
                    ((line++))
                elif [[ $REPLY =~ ^\- ]]; then
                    flags="$flags:$line|red|-"
                fi
            done
            echo "setb git_diff_flags '$flags'"
        }
    }

    case "$1" in
       show|log|diff) show_git_cmd_output "$@" ;;
       blame) run_git_blame ;;
       show-diff)
           echo "try %{ addhl flag_lines black git_diff_flags } catch %{}"
           update_diff
           ;;
       update-diff) update_diff ;;
       add)
           name="${2:-${kak_bufname}}"
           if git add -- "${name}"; then
              echo "echo -color Information 'git: added ${name}'"
           else
              echo "echo -color Error 'git: unable to add ${name}'"
           fi
           ;;
       *) echo "echo %{unknown git command '$1'}"; exit ;;
    esac

}}
