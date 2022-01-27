declare-option -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

hook -group git-log-highlight global WinSetOption filetype=git-log %{
    add-highlighter window/git-log group
    add-highlighter window/git-log/ regex '^([*|\\ /_.-])*' 0:keyword
    add-highlighter window/git-log/ regex '^( ?[*|\\ /_.-])*\h{,3}(commit )?(\b[0-9a-f]{4,40}\b)' 2:keyword 3:comment
    add-highlighter window/git-log/ regex '^( ?[*|\\ /_.-])*\h{,3}([a-zA-Z_-]+:) (.*?)$' 2:variable 3:value
    add-highlighter window/git-log/ ref diff # highlight potential diffs from the -p option

    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-log }
}

hook -group git-status-highlight global WinSetOption filetype=git-status %{
    add-highlighter window/git-status group
    add-highlighter window/git-status/ regex '^## ' 0:comment
    add-highlighter window/git-status/ regex '^## (\S*[^\s\.@])' 1:green
    add-highlighter window/git-status/ regex '^## (\S*[^\s\.@])(\.\.+)(\S*[^\s\.@])' 1:green 2:comment 3:red
    add-highlighter window/git-status/ regex '^(##) (No commits yet on) (\S*[^\s\.@])' 1:comment 2:Default 3:green
    add-highlighter window/git-status/ regex '^## \S+ \[[^\n]*ahead (\d+)[^\n]*\]' 1:green
    add-highlighter window/git-status/ regex '^## \S+ \[[^\n]*behind (\d+)[^\n]*\]' 1:red
    add-highlighter window/git-status/ regex '^(?:([Aa])|([Cc])|([Dd!?])|([MUmu])|([Rr])|([Tt]))[ !\?ACDMRTUacdmrtu]\h' 1:green 2:blue 3:red 4:yellow 5:cyan 6:cyan
    add-highlighter window/git-status/ regex '^[ !\?ACDMRTUacdmrtu](?:([Aa])|([Cc])|([Dd!?])|([MUmu])|([Rr])|([Tt]))\h' 1:green 2:blue 3:red 4:yellow 5:cyan 6:cyan
    add-highlighter window/git-status/ regex '^R[ !\?ACDMRTUacdmrtu] [^\n]+( -> )' 1:cyan
    add-highlighter window/git-status/ regex '^\h+(?:((?:both )?modified:)|(added:|new file:)|(deleted(?: by \w+)?:)|(renamed:)|(copied:))(?:.*?)$' 1:yellow 2:green 3:red 4:cyan 5:blue 6:magenta

    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-status }
}

declare-option -hidden line-specs git_blame_flags
declare-option -hidden line-specs git_diff_flags
declare-option -hidden int-list git_hunk_list

define-command -params 1.. \
    -docstring %{
        git [<arguments>]: git wrapping helper
        All the optional arguments are forwarded to the git utility
        Available commands:
            add
            rm
            blame
            commit
            checkout
            diff
            hide-blame
            hide-diff
            init
            log
            next-hunk
            previous-hunk
            show
            show-diff
            status
            update-diff
    } -shell-script-candidates %{
    if [ $kak_token_to_complete -eq 0 ]; then
        printf "add\nrm\nblame\ncommit\ncheckout\ndiff\nhide-blame\nhide-diff\nlog\nnext-hunk\nprev-hunk\nshow\nshow-diff\ninit\nstatus\nupdate-diff\n"
    else
        case "$1" in
            commit) printf -- "--amend\n--no-edit\n--all\n--reset-author\n--fixup\n--squash\n"; git ls-files -m ;;
            add) git ls-files -dmo --exclude-standard ;;
            rm) git ls-files -c ;;
        esac
    fi
  } \
  git %{ evaluate-commands %sh{
    cd_bufdir() {
        dirname_buffer="${kak_buffile%/*}"
        cd "${dirname_buffer}" 2>/dev/null || {
            printf 'fail Unable to change the current working directory to: %s\n' "${dirname_buffer}"
            exit 1
        }
    }

    show_git_cmd_output() {
        local filetype
        local map_diff_goto_source

        case "$1" in
           diff) map_diff_goto_source=true; filetype=diff ;;
           show) map_diff_goto_source=true; filetype=git-log ;;
           log)  filetype=git-log ;;
           status)  filetype=git-status ;;
           *) return 1 ;;
        esac
        output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-git.XXXXXXXX)/fifo
        mkfifo ${output}
        ( git "$@" > ${output} 2>&1 & ) > /dev/null 2>&1 < /dev/null

        # We need to unmap in case an existing buffer changes type,
        # for example if the user runs "git show" and "git status".
        map_diff_goto_source=$([ -n "${map_diff_goto_source}" ] \
          && printf %s "map buffer normal <ret> %[: git-diff-goto-source<ret>] -docstring 'Jump to source from git diff'" \
          || printf %s "unmap buffer normal <ret> %[: git-diff-goto-source<ret>]")

        printf %s "evaluate-commands -try-client '$kak_opt_docsclient' %{
                  edit! -fifo ${output} *git*
                  set-option buffer filetype '${filetype}'
                  hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -r $(dirname ${output}) } }
                  ${map_diff_goto_source}
              }"
    }

    run_git_blame() {
        (
            cd_bufdir
            printf %s "evaluate-commands -client '$kak_client' %{
                      try %{ add-highlighter window/git-blame flag-lines Information git_blame_flags }
                      set-option buffer=$kak_bufname git_blame_flags '$kak_timestamp'
                  }" | kak -p ${kak_session}
                  git blame "$@" --incremental ${kak_buffile} | awk '
                  function send_flags(flush,    text, i) {
                      if (line == "") { return; }
                      text=substr(sha,1,8) " " dates[sha] " " authors[sha]
                      # gsub("|", "\\|", text)
                      gsub("~", "~~", text)
                      for ( i=0; i < count; i++ ) {
                          flags = flags " %~" line+i "|" text "~"
                      }
                      now = systime()
                      # Send roughly one update per second, to avoid creating too many kak processes.
                      if (!flush && now - last_sent < 1) {
                          return
                      }
                      cmd = "kak -p " ENVIRON["kak_session"]
                      print "set-option -add buffer=" ENVIRON["kak_bufname"] " git_blame_flags " flags | cmd
                      close(cmd)
                      flags = ""
                      last_sent = now
                  }
                  /^([0-9a-f]+) ([0-9]+) ([0-9]+) ([0-9]+)/ {
                      send_flags(0)
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
                  END { send_flags(1); }'
        ) > /dev/null 2>&1 < /dev/null &
    }

    run_git_cmd() {
        if git "${@}" > /dev/null 2>&1; then
          printf %s "echo -markup '{Information}git $1 succeeded'"
        else
          printf 'fail git %s failed\n' "$1"
        fi
    }

    update_diff() {
        (
            cd_bufdir
            git --no-pager diff -U0 "$kak_buffile" | perl -e '
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

    jump_hunk() {
        direction=$1
        set -- ${kak_opt_git_diff_flags}
        shift

        if [ $# -lt 1 ]; then
            echo "fail 'no git hunks found'"
            exit
        fi

        # Update hunk list if required
        if [ "$kak_timestamp" != "${kak_opt_git_hunk_list%% *}" ]; then
            hunks=$kak_timestamp

            prev_line="-1"
            for line in "$@"; do
                line="${line%%|*}"
                if [ "$((line - prev_line))" -gt 1 ]; then
                    hunks="$hunks $line"
                fi
                prev_line="$line"
            done
            echo "set-option buffer git_hunk_list $hunks"
            hunks=${hunks#* }
        else
            hunks=${kak_opt_git_hunk_list#* }
        fi

        prev_hunk=""
        next_hunk=""
        for hunk in ${hunks}; do
            if   [ "$hunk" -lt "$kak_cursor_line" ]; then
                prev_hunk=$hunk
            elif [ "$hunk" -gt "$kak_cursor_line" ]; then
                next_hunk=$hunk
                break
            fi
        done

        wrapped=false
        if [ "$direction" = "next" ]; then
            if [ -z "$next_hunk" ]; then
                next_hunk=${hunks%% *}
                wrapped=true
            fi
            if [ -n "$next_hunk" ]; then
                echo "select $next_hunk.1,$next_hunk.1"
            fi
        elif [ "$direction" = "prev" ]; then
            if [ -z "$prev_hunk" ]; then
                wrapped=true
                prev_hunk=${hunks##* }
            fi
            if [ -n "$prev_hunk" ]; then
                echo "select $prev_hunk.1,$prev_hunk.1"
            fi
        fi

        if [ "$wrapped" = true ]; then
            echo "echo -markup '{Information}git hunk search wrapped around buffer'"
        fi
    }

    commit() {
        # Handle case where message needs not to be edited
        if grep -E -q -e "-m|-F|-C|--message=.*|--file=.*|--reuse-message=.*|--no-edit|--fixup.*|--squash.*"; then
            if git commit "$@" > /dev/null 2>&1; then
                echo 'echo -markup "{Information}Commit succeeded"'
            else
                echo 'fail Commit failed'
            fi
            exit
        fi <<-EOF
			$@
		EOF

        # fails, and generate COMMIT_EDITMSG
        GIT_EDITOR='' EDITOR='' git commit "$@" > /dev/null 2>&1
        msgfile="$(git rev-parse --git-dir)/COMMIT_EDITMSG"
        printf %s "edit '$msgfile'
              hook buffer BufWritePost '.*\Q$msgfile\E' %{ evaluate-commands %sh{
                  if git commit -F '$msgfile' --cleanup=strip $* > /dev/null; then
                     printf %s 'evaluate-commands -client $kak_client echo -markup %{{Information}Commit succeeded}; delete-buffer'
                  else
                     printf 'evaluate-commands -client %s fail Commit failed\n' "$kak_client"
                  fi
              } }"
    }

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
        next-hunk) jump_hunk next ;;
        prev-hunk) jump_hunk prev ;;
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
            printf "fail unknown git command '%s'\n" "$1"
            exit
            ;;
    esac
}}

# Options needed by git-diff-goto-source command
declare-option -hidden str git_diff_hunk_filename
declare-option -hidden int git_diff_hunk_line_num_start
declare-option -hidden int git_diff_go_to_line_num
declare-option -hidden str git_diff_git_dir
declare-option -hidden str git_diff_section_heading
declare-option -hidden int git_diff_cursor_column

# Works within :git diff and :git show
define-command git-diff-goto-source \
    -docstring 'Navigate to source by pressing the enter key in hunks when git diff is displayed. Works within :git diff and :git show' %{
    try %{
        set-option global git_diff_git_dir %sh{
           git rev-parse --show-toplevel
        }
        # We will need this later. Need to subtract 1 because a diff has an initial column
        # for -,+,<space>
        set-option global git_diff_cursor_column %sh{ echo $(($kak_cursor_column-1)) }

        # This function works_within a hunk or in the diff header.
        #   - On a context line or added line, it will navigate to that line.
        #   - On a deleted line, it will navigate to the context line or added line just above.
        #   - On a @@ line (i.e. a "hunk header") this will navigate to section heading (see below).
        #   - A diff header contains lines starting with "diff", "index", "+++", and "---".
        #     Inside a diff header, this will navigate to the first line of the file.
        execute-keys -draft 'x<a-k>^[@ +-]|^diff|^index<ret>'

        # Find the source filename for the current hunk (reverse search)
        evaluate-commands -draft %{
            # First look for the "diff" line. because "+++" may be part of a diff.
            execute-keys 'x<semicolon><a-/>^diff<ret></>^\+\+\+ \w([^\n]*)<ret>'
            set-option global git_diff_hunk_filename %reg{1}
        }

        try %{
            # Are we inside the diff header? If so simply go to the first line of the file.
            # The diff header is everything before the first hunk header.
            execute-keys -draft 'x<semicolon><a-?>^diff<ret><a-K>^@@<ret>'
            edit -existing "%opt{git_diff_git_dir}%opt{git_diff_hunk_filename}" 1
        } catch %{
            # Find the source line at which the current hunk starts (reverse search)
            evaluate-commands -draft %{
                execute-keys 'x<semicolon><a-/>^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@<ret>'
                set-option buffer git_diff_hunk_line_num_start %reg{1}
            }
            # If we're already on a hunk header (i.e. a line that starts with @@) then
            # our behavior changes slightly: we need to go look for the section heading.
            # For example take this hunk header:  @@ -123,4 +123,4 @@ fn some_function_name_possibly
            # Here the section heading is "fn some_function_name_possibly". Please note that the section
            # heading is NOT necessarily at the hunk start line so we can't trivially extract that.
            try %{
                # First things first, are we on a hunk header? If not, head to the nearest `catch`
                execute-keys -draft 'x<a-k>^@@<ret>'
                evaluate-commands -try-client %opt{jumpclient} %{
                    # Now attempt to find the section heading!
                    try %{
                        # First extract the section heading.
                        evaluate-commands -draft %{
                            execute-keys 'xs^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@ ([^\n]*)<ret>'
                            set-option global git_diff_section_heading %reg{2}
                        }
                        # Go to the hunk start in the source file. The section header will be above us.
                        edit -existing "%opt{git_diff_git_dir}%opt{git_diff_hunk_filename}" %opt{git_diff_hunk_line_num_start}
                        # Search for the raw text of the section, like "fn some_function_name_possibly". That should work most of the time.
                        set-register / "\Q%opt{git_diff_section_heading}"
                        # Search backward from where the cursor is now.
                        # Note that the hunk line number is NOT located at the same place as the section heading.
                        # After we have found it, adjust the cursor and center the viewport as if we had directly jumped
                        # to the first character of the section header with and `edit` command.
                        execute-keys "<a-/><ret><a-semicolon><semicolon>vc"
                    } catch %{
                        # There is no section heading, or we can't find it in the source file,
                        # so just go to the hunk start line.
                        # NOTE that we DONT go to the saved cursor column,
                        # because our cursor column will be fixed to the start of the section heading
                        edit -existing "%opt{git_diff_git_dir}%opt{git_diff_hunk_filename}" %opt{git_diff_hunk_line_num_start}
                    }
                }
           } catch %{
                # This catch deals with the typical case. We're somewhere on either:
                # (a) A context line i.e. lines starting with ' '
                # or (b) On a line removal i.e. lines starting with '-'
                # or (c) On a line addition i.e. lines starting with '+'
                # So now try to figure out a line offset + git_diff_hunk_line_num_start that we need to go to
                # Ignoring any diff lines starting with `-`, how many lines from where we
                # pressed <ret> till the start of the hunk?
                evaluate-commands -draft %{
                   execute-keys '<a-?>^@@<ret>J<a-s><a-K>^-<ret>'
                   set-option global git_diff_go_to_line_num %sh{
                       set -- $kak_reg_hash
                       line=$(($#+$kak_opt_git_diff_hunk_line_num_start-1))
                       echo $line
                   }
                }
                evaluate-commands -try-client %opt{jumpclient} %{
                    # Open the source file at the appropriate line number and cursor column
                    edit -existing "%opt{git_diff_git_dir}%opt{git_diff_hunk_filename}" %opt{git_diff_go_to_line_num} %opt{git_diff_cursor_column}
                }
            }
        }
    } catch %{
        fail "git-diff-goto-source: unable to navigate to source. Use only inside a diff"
    }
}
