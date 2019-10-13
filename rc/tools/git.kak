declare-option -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

hook -group git-log-highlight global WinSetOption filetype=git-log %{
    add-highlighter window/git-log group
    add-highlighter window/git-log/ regex '^(commit) ([0-9a-f]+)( [^\n]+)?$' 1:keyword 2:meta 3:comment
    add-highlighter window/git-log/ regex '^([a-zA-Z_-]+:) (.*?)$' 1:variable 2:value
    add-highlighter window/git-log/ ref diff # highlight potential diffs from the -p option

    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-log }
}

hook -group git-status-highlight global WinSetOption filetype=git-status %{
    add-highlighter window/git-status group
    add-highlighter window/git-status/ regex '^\h+(?:((?:both )?modified:)|(added:|new file:)|(deleted(?: by \w+)?:)|(renamed:)|(copied:))(?:.*?)$' 1:yellow 2:green 3:red 4:cyan 5:blue 6:magenta

    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-status }
}

declare-option -hidden line-specs git_blame_flags
declare-option -hidden line-specs git_diff_flags

define-command -params 1.. \
  -docstring %sh{printf 'git [<arguments>]: git wrapping helper
All the optional arguments are forwarded to the git utility
Available commands:\n  add\n  rm\n  blame\n  commit\n  checkout\n  diff\n  hide-blame\n  hide-diff\n  init\n  log\n  show\n  show-diff\n  status\n  update-diff'} \
  -shell-script-candidates %{
    i=0
    state='want_command'
    while [ $i -lt $kak_token_to_complete ] && [ $# -gt 0 ]; do
        case "$state:$1" in
            want_command:-C) state='want_path';;
            want_path:*)     state='want_command';;
            want_command:-*) state='want_command';;
            want_command:*)  state="$1" ; break ;;
        esac
        shift
        i=$(( i + 1 ))
    done
    case "$state" in
        want_command) printf "add\nrm\nblame\ncommit\ncheckout\ndiff\nhide-blame\nhide-diff\nlog\nshow\nshow-diff\ninit\nstatus\nupdate-diff\n" ;;
        want_path)    ls -1d */ |tr -d / ;;
        commit)       printf -- "--amend\n--no-edit\n--all\n--reset-author\n--fixup\n--squash\n"; run_git ls-files -m ;;
        add)          run_git ls-files -dmo --exclude-standard ;;
        rm)           run_git ls-files -c ;;
    esac
  } \
  git %{ evaluate-commands %sh{
    cd_bufdir() {
        dirname_buffer="${kak_buffile%/*}"
        cd "${dirname_buffer}" 2>/dev/null || {
            printf 'echo -markup {Error}Unable to change the current working directory to: %s' "${dirname_buffer}"
            exit 1
        }
    }

    show_git_cmd_output() {
        local filetype
        case "$1" in
           show|diff) filetype=diff ;;
           log)  filetype=git-log ;;
           status)  filetype=git-status ;;
        esac
        output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-git.XXXXXXXX)/fifo
        mkfifo ${output}
        ( run_git "$@" > ${output} 2>&1 & ) > /dev/null 2>&1 < /dev/null

        printf %s "evaluate-commands -try-client '$kak_opt_docsclient' %{
                  edit! -fifo ${output} *git*
                  set-option buffer filetype '${filetype}'
                  hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -r $(dirname ${output}) } }
              }"
    }

    run_git_blame() {
        (
            cd_bufdir
            printf %s "evaluate-commands -client '$kak_client' %{
                      try %{ add-highlighter window/git-blame flag-lines Information git_blame_flags }
                      set-option buffer=$kak_bufname git_blame_flags '$kak_timestamp'
                  }" | kak -p ${kak_session}
                  run_git blame "$@" --incremental ${kak_buffile} | awk '
                  function send_flags(text, flag, i) {
                      if (line == "") { return; }
                      text=substr(sha,1,8) " " dates[sha] " " authors[sha]
                      # gsub("|", "\\|", text)
                      gsub("~", "~~", text)
                      flag="%~" line "|" text "~"
                      for ( i=1; i < count; i++ ) {
                          flag=flag " %~" line+i "|" text "~"
                      }
                      cmd = "kak -p " ENVIRON["kak_session"]
                      print "set-option -add buffer=" ENVIRON["kak_bufname"] " git_blame_flags " flag | cmd
                      close(cmd)
                  }
                  /^([0-9a-f]+) ([0-9]+) ([0-9]+) ([0-9]+)/ {
                      send_flags()
                      sha=$1
                      line=$3
                      count=$4
                  }
                  /^author / { authors[sha]=substr($0,8) }
                  /^author-time ([0-9]*)/ {
                       cmd = "date -d @" $2 " +\"%F %T\""
                       cmd | getline dates[sha]
                       close(cmd)
                  }
                  END { send_flags(); }'
        ) > /dev/null 2>&1 < /dev/null &
    }

    run_git() {
        git $git_driver_args "${@}"
    }

    run_git_cmd() {
        if run_git "${@}" > /dev/null 2>&1; then
          printf %s "echo -markup '{Information}git $1 succeeded'"
        else
          printf %s "echo -markup '{Error}git $1 failed'"
        fi
    }

    update_diff() {
        (
            cd_bufdir
            run_git --no-pager diff -U0 "$kak_buffile" | perl -e '
            $flags = $ENV{"kak_timestamp"};
            foreach $line (<STDIN>) {
                if ($line =~ /@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))?/) {
                    $from_line = $1;
                    $from_count = ($2 eq "" ? 1 : $2);
                    $to_line = $3;
                    $to_count = ($4 eq "" ? 1 : $4);

                    if ($from_count == 0 and $to_count > 0) {
                        for $i (0..$to_count - 1) {
                            $line = $to_line + $i;
                            $flags .= " $line|\{green\}+";
                        }
                    }
                    elsif ($from_count > 0 and $to_count == 0) {
                        if ($to_line == 0) {
                            $flags .= " 1|\{red\}‾";
                        } else {
                            $flags .= " $to_line|\{red\}_";
                        }
                    }
                    elsif ($from_count > 0 and $from_count == $to_count) {
                        for $i (0..$to_count - 1) {
                            $line = $to_line + $i;
                            $flags .= " $line|\{blue\}~";
                        }
                    }
                    elsif ($from_count > 0 and $from_count < $to_count) {
                        for $i (0..$from_count - 1) {
                            $line = $to_line + $i;
                            $flags .= " $line|\{blue\}~";
                        }
                        for $i ($from_count..$to_count - 1) {
                            $line = $to_line + $i;
                            $flags .= " $line|\{green\}+";
                        }
                    }
                    elsif ($to_count > 0 and $from_count > $to_count) {
                        for $i (0..$to_count - 2) {
                            $line = $to_line + $i;
                            $flags .= " $line|\{blue\}~";
                        }
                        $last = $to_line + $to_count - 1;
                        $flags .= " $last|\{blue+u\}~";
                    }
                }
            }
            print "set-option buffer git_diff_flags $flags"
        ' )
    }

    commit() {
        # Handle case where message needs not to be edited
        if grep -E -q -e "-m|-F|-C|--message=.*|--file=.*|--reuse-message=.*|--no-edit|--fixup.*|--squash.*"; then
            if run_git commit "$@" > /dev/null 2>&1; then
                echo 'echo -markup "{Information}Commit succeeded"'
            else
                echo 'echo -markup "{Error}Commit failed"'
            fi
            exit
        fi <<-EOF
			$@
		EOF

        # fails, and generate COMMIT_EDITMSG
        GIT_EDITOR='' EDITOR='' run_git commit "$@" > /dev/null 2>&1
        msgfile="$(git rev-parse --git-dir)/COMMIT_EDITMSG"
        printf %s "edit '$msgfile'
              hook buffer BufWritePost '.*\Q$msgfile\E' %{ evaluate-commands %sh{
                  if git $git_driver_args commit -F '$msgfile' --cleanup=strip $* > /dev/null; then
                     printf %s 'evaluate-commands -client $kak_client echo -markup %{{Information}Commit succeeded}; delete-buffer'
                  else
                     printf %s 'evaluate-commands -client $kak_client echo -markup %{{Error}Commit failed}'
                  fi
              } }"
    }

    while [ $# -gt 0 ]; do
        case "$1" in
            -C)
                git_driver_args="$1 $2"
                shift 2
                ;;
            -C*)
                git_driver_args="$1"
                shift 1
                ;;
            *)
                break
                ;;
        esac
    done

    case "$1" in
        show|log|diff|status)
            show_git_cmd_output "$@"
            ;;
        blame)
            shift
            run_git_blame "$@"
            ;;
        hide-blame)
            printf %s "try %{
                set-option buffer=$kak_bufname git_blame_flags $kak_timestamp
                remove-highlighter window/git-blame
            }"
            ;;
        show-diff)
            echo 'try %{ add-highlighter window/git-diff flag-lines Default git_diff_flags }'
            update_diff
            ;;
        hide-diff)
            echo 'try %{ remove-highlighter window/git-diff }'
            ;;
        update-diff) update_diff ;;
        commit)
            shift
            commit "$@"
            ;;
        init)
            shift
            git init "$@" > /dev/null 2>&1
            ;;
        add|rm)
            cmd="$1"
            shift
            run_git_cmd $cmd "${@:-${kak_buffile}}"
            ;;
        reset|checkout)
            run_git_cmd "$@"
            ;;
        *)
            printf %s "echo -markup %{{Error}unknown git command '$1'}"
            exit
            ;;
    esac
}}
