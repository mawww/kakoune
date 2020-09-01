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

define-command -params .. -docstring %{
    git-add [<file>]...: add the given files, or the current buffer if unspecified
} -shell-script-candidates %{
    git ls-files -dmo --exclude-standard
} git-add %{ evaluate-commands %sh{
    if [ $# -lt 1 ]; then
        set -- "${kak_buffile}"
    fi

    if git add "$@" >/dev/null; then
        printf 'echo -markup -- {Information}File(s) added: %s' "$*"
    else
        printf 'fail -- Unable to add file(s): %s' "$*"
    fi
} }

define-command -params .. -docstring %{
    git-rm [<file>]...: remove the given files, or the current buffer if unspecified
} -shell-script-candidates %{
    git ls-files -c
} git-rm %{ evaluate-commands %sh{
    if [ $# -lt 1 ]; then
        set -- "${kak_buffile}"
    fi

    if git rm "$@" >/dev/null; then
        printf 'echo -markup -- {Information}File(s) removed: %s' "$*"
    else
        printf 'fail -- Unable to remove file(s): %s' "$*"
    fi
} }

define-command -params .. -docstring %{
    git-blame [<args>]: show author/revision information in the buffer

    All optional arguments are forwarded to `git blame`
} git-blame %{ evaluate-commands %sh{
    cd_bufdir() {
        dirname_buffer="${kak_buffile%/*}"
        if ! cd "${dirname_buffer}" 2>/dev/null; then
            printf 'fail -- Unable to change the current working directory to: %s' "${dirname_buffer}"
            exit 1
        fi
    }

    (
        cd_bufdir
        printf %s "evaluate-commands -client '$kak_client' %{
                  try %{ add-highlighter window/git-blame flag-lines Information git_blame_flags }
                  set-option \"buffer=$kak_bufname\" git_blame_flags '$kak_timestamp'
              }" | kak -p "${kak_session}"
        git blame "$@" --incremental "${kak_buffile}" | awk '
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
} }

define-command -docstring %{
    git-hide-blame: hide revision/author information in the buffer
} git-hide-blame %{ try %{
    set-option "buffer=%val{bufname}" git_blame_flags %val{timestamp}
    remove-highlighter window/git-blame
} }

define-command -params .. -docstring %{
    git-commit [<file>]...: commit all the given files
} -shell-script-candidates %{
    printf -- "--amend\n--no-edit\n--all\n--reset-author\n--fixup\n--squash\n"
    git ls-files -m
} git-commit %{ evaluate-commands %sh{
    # Handle case where message needs not to be edited
    if grep -E -q -e "-m|-F|-C|--message=.*|--file=.*|--reuse-message=.*|--no-edit|--fixup.*|--squash.*"; then
        if git commit "$@" >/dev/null; then
            printf 'echo -markup -- {Information}File(s) committed: %s' "$*"
        else
            printf 'fail -- Unable to commit file(s): %s' "$*"
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
                 printf %s 'evaluate-commands -client $kak_client echo -markup -- %{{Information}File(s) committed: $*}; delete-buffer'
              else
                 printf 'evaluate-commands -client %s fail -- Unable to commit file(s): $*' "$kak_client"
              fi
          } }"
} }

define-command -params .. -docstring %{
    git-checkout [<file>]...: check out the given files
} git-checkout %{ evaluate-commands %sh{
    if git checkout "$@" >/dev/null; then
        printf 'echo -markup -- {Information}File(s) checked out: %s' "$*"
    else
        printf 'fail -- Unable to check out file(s): %s' "$*"
    fi
} }

define-command -params .. -docstring %{
    git-diff [<file>]...: show the diff of the given files
} git-diff %{ evaluate-commands %sh{
    output=$(mktemp "${TMPDIR:-/tmp}"/kak-git-diff.XXXXXXXX)
    rm "${output}"
    mkfifo "${output}"
    ( git diff "$@" > "${output}" 2>&1 & ) > /dev/null 2>&1 < /dev/null

    printf %s "evaluate-commands -try-client '$kak_opt_docsclient' %{
              edit! -fifo \"${output}\" *git-diff*
              set-option buffer filetype diff
              hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -f \"${output}\" } }
          }"
} }

define-command -docstring %{
    Hide the modification flags for the buffer's contents
} git-hide-diff %{
    try %{ remove-highlighter window/git-diff }
}

define-command -params .. -docstring %{
    git-init [<arg>]..: initialise a repository

    All optional arguments are forwarded to `git init`
} git-init %{ evaluate-commands %sh{
    if git init "$@" >/dev/null; then
        printf 'echo -markup -- {Information}Repository initialised'
    else
        printf 'fail -- Unable to initialise the repository'
    fi
} }

define-command -params .. -docstring %{
    git-log [<file>]...: show the log of the given files
} git-log %{ evaluate-commands %sh{
    output=$(mktemp "${TMPDIR:-/tmp}"/kak-git-log.XXXXXXXX)
    rm "${output}"
    mkfifo "${output}"
    ( git log "$@" > "${output}" 2>&1 & ) > /dev/null 2>&1 < /dev/null

    printf %s "evaluate-commands -try-client '$kak_opt_docsclient' %{
              edit! -fifo \"${output}\" *git-log*
              set-option buffer filetype git-log
              hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -f \"${output}\" } }
          }"
} }

define-command -docstring %{
    Jump to the next hunk
} -shell-script-candidates %{
} git-next-hunk %{ evaluate-commands %sh{
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

    jump_hunk next
} }

define-command -docstring %{
    Jump to the previous hunk
} -shell-script-candidates %{
} git-previous-hunk %{ evaluate-commands %sh{
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

    jump_hunk prev
} }

define-command -params .. -docstring %{
    git-reset [<file>]...: reset the given files
} git-reset %{ evaluate-commands %sh{
    if git reset "$@" >/dev/null; then
        printf 'echo -markup -- {Information}File(s) reset: %s' "$*"
    else
        printf 'fail -- Unable to reset file(s): %s' "$*"
    fi
} }

define-command -params .. -docstring %{
    git-show [<file>]...: show the given files
} git-show %{ evaluate-commands %sh{
    output=$(mktemp "${TMPDIR:-/tmp}"/kak-git-show.XXXXXXXX)
    rm "${output}"
    mkfifo "${output}"
    ( git show "$@" > "${output}" 2>&1 & ) > /dev/null 2>&1 < /dev/null

    printf %s "evaluate-commands -try-client '$kak_opt_docsclient' %{
              edit! -fifo \"${output}\" *git-show*
              set-option buffer filetype git-log
              hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -f \"${output}\" } }
          }"
} }

define-command -docstring %{
    Show the modification flags for the buffer's contents
} git-show-diff %{
    try %{ add-highlighter window/git-diff flag-lines Default git_diff_flags }

    evaluate-commands %sh{
        cd_bufdir() {
            dirname_buffer="${kak_buffile%/*}"
            if ! cd "${dirname_buffer}" 2>/dev/null; then
                printf 'fail -- Unable to change the current working directory to: %s' "${dirname_buffer}"
                exit 1
            fi
        }

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
}

define-command -params .. -docstring %{
    git-status [<file>]...: show the status of the given files
} git-status %{ evaluate-commands %sh{
    output=$(mktemp "${TMPDIR:-/tmp}"/kak-git-status.XXXXXXXX)
    rm "${output}"
    mkfifo "${output}"
    ( git status "$@" > "${output}" 2>&1 & ) > /dev/null 2>&1 < /dev/null

    printf %s "evaluate-commands -try-client '$kak_opt_docsclient' %{
              edit! -fifo \"${output}\" *git-status*
              set-option buffer filetype git-status
              hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -f \"${output}\" } }
          }"
} }

define-command -docstring %{
    Update the diff of the current buffer
} git-update-diff %{ evaluate-commands %sh{
    cd_bufdir() {
        dirname_buffer="${kak_buffile%/*}"
        if ! cd "${dirname_buffer}" 2>/dev/null; then
            printf 'fail -- Unable to change the current working directory to: %s' "${dirname_buffer}"
            exit 1
        fi
    }

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
} }
