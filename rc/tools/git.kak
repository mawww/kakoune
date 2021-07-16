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
        local apply_goto_line_map
        local maybe_goto_line_map

        case "$1" in
           diff) apply_goto_line_map='true'; filetype=diff ;;
           show) apply_goto_line_map='true'; filetype=git-log ;;
           log)  filetype=git-log ;;
           status)  filetype=git-status ;;
           *) return 1 ;;
        esac
        output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-git.XXXXXXXX)/fifo
        mkfifo ${output}
        ( git "$@" > ${output} 2>&1 & ) > /dev/null 2>&1 < /dev/null

        if [ "$apply_goto_line_map" = 'true' ]; then
            maybe_goto_source_map="map -docstring 'Jump to source from git diff' buffer normal <ret> %{: git-diff-goto-source<ret>}"
        else
            maybe_goto_source_map=''
        fi

        printf %s "evaluate-commands -try-client '$kak_opt_docsclient' %{
                  edit! -fifo ${output} *git*
                  set-option buffer filetype '${filetype}'
                  hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -r $(dirname ${output}) } }
                  ${maybe_goto_source_map}
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
declare-option -hidden str git_diff_cursor_column

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

        # Allowed to press <ret> only _within_ a hunk
        # Note: - Pressing <ret> on a @@ line (i.e. a "range line") is allowed.
        #         This action will result in navigating to section heading (see below).
        #       - Pressing <ret> on a +++ line is allowed. This action will result in
        #         navigating to the first line of the file
        execute-keys -draft 'x<a-K>^\w<ret>'
        execute-keys -draft 'x<a-K>^$<ret>'
        execute-keys -draft 'x<a-K>^---<ret>'

        # Find the source filename for the current hunk (reverse search)
        evaluate-commands -draft %{
            execute-keys 'x<semicolon><a-/>^\+\+\+ b([^\n]*)<ret>'
            set-option global git_diff_hunk_filename %reg{1}
        }

        try %{
            # Are we on a +++ line? If so simply go to the first line of the file
            execute-keys -draft 'x<a-k>^\+\+\+<ret>'
            # Goto line 1 as we pressed on +++
            edit -existing "%opt{git_diff_git_dir}%opt{git_diff_hunk_filename}" 1
        } catch %{
            # Find the source line at which the current hunk starts (reverse search)
            evaluate-commands -draft %{
                execute-keys 'x<semicolon><a-/>^@@ -\d+,\d+ \+(\d+),\d+ @@<ret>'
                set-option buffer git_diff_hunk_line_num_start %reg{1}
            }
            # If we're already on a range line (i.e. a line that starts with @@) when
            # <ret> was pressed our behavior slightly alters: we need to go look for the section heading.
            # For example for range line:  @@ -123,4 +123,4 @@ fn some_function_name_possibly
            # Here the section heading is "fn some_function_name_possibly". Please note that the section
            # heading is NOT at line range line number necessarily so we can't trivially extract that.
            try %{
                # First things first, are we on a range line? If not, head to the nearest `catch`
                execute-keys -draft 'x<a-k>^@@<ret>'
                # Sane default. This is where we will go if:
                # (a) section heading could not be found for some reason (b) section heading does not exist
                set-option global git_diff_go_to_line_num %opt{git_diff_hunk_line_num_start}

                try %{
                    # Now search for the section heading!
                    #
                    # We're doing something a bit weird here: we're editing the file in a draft context!
                    # The reason is that first we open up the file and _then_ we try to navigate to the
                    # section heading. This adds two locations to the jump list. We ideally should have gone
                    # to the section heading in one step but didn't know its line number in advance.
                    #
                    # Once we get to the section heading, save the line number. Then in a *non-draft* context
                    # navigate again in a single step so that only one entry is saved to the jump list.
                    evaluate-commands -draft %{
                        execute-keys 'xs^@@ -\d+,\d+ \+(\d+),\d+ @@ ([^\n]*)<ret>'
                        set-option global git_diff_section_heading %reg{2}

                        # Open the source file at line number found from the range.
                        # As discussed above we need to now search for the section heading
                        edit -existing "%opt{git_diff_git_dir}%opt{git_diff_hunk_filename}" %opt{git_diff_go_to_line_num}
                        # Try to search for the section heading. If for some reason that fails
                        # then we automatically fall back to the the currently saved git_diff_go_to_line_num
                        try %{
                            # We also need to navigate to the context!
                            # Note the use of \Q so we don't need to quote special regex characters
                            set-register / "\Q%opt{git_diff_section_heading}"
                            # This searches backward from where the cursor is now
                            # Note that the hunk line number is NOT located at the same place as the section heading
                            execute-keys "<a-/><ret>"

                            set-option global git_diff_go_to_line_num %val{cursor_line}
                        }
                    }
                }
                # Whether or not we found the section heading we now have a line to go to
                evaluate-commands -try-client %opt{jumpclient} %{
                    # Open the source file at the appropriate line number! NOTE that we DONT go to the saved cursor column
                    # because our cursor column will be fixed to the start of the section heading
                    edit -existing "%opt{git_diff_git_dir}%opt{git_diff_hunk_filename}" %opt{git_diff_go_to_line_num}
                }
           } catch %{
                # This catch deals with the typical case. We're somewhere within either:
                # (a) The context area i.e. lines starting with ' '
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
        fail "Unable to navigate to source! You can press the `enter` key only inside a hunk, on a range line (`@@ ... @@`) and on a `+++` line"
    }
}
