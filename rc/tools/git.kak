declare-option -docstring 'Name of the client in which documentation is to be displayed' \
    str docsclient

hook -group git-log-highlight global WinSetOption filetype=git-log %{
    add-highlighter window/git-log group
    add-highlighter window/git-log/ regex '^(commit) ([0-9a-f]+)( [^\n]+)?$' 1:keyword 2:meta 3:comment
    add-highlighter window/git-log/ regex '^([a-zA-Z_-]+:) (.*?)$' 1:variable 2:value
    add-highlighter window/git-log/ ref diff # Highlight potential diffs from the --patch option
    hook -always -once window WinSetOption filetype=.* %{ remove-highlighter window/git-log }
}

hook -group git-status-highlight global WinSetOption filetype=git-status %{
    add-highlighter window/git-status group
    add-highlighter window/git-status/ regex '^\h+(?:((?:both )?modified:)|(added:|new file:)|(deleted(?: by \w+)?:)|(renamed:)|(copied:))(?:.*?)$' 1:yellow 2:green 3:red 4:cyan 5:blue 6:magenta
    hook -always -once window WinSetOption filetype=.* %{ remove-highlighter window/git-status }
}

declare-option -hidden line-specs git_blame_flags
declare-option -hidden line-specs git_diff_flags

define-command -params 1.. \
    -docstring %{
git [<arguments>]: Git wrapping helper
All the optional arguments are forwarded to the Git utility
Available commands:
– add
– blame
– checkout
– commit
– diff
– hide-blame
– hide-diff
– init
– log
– rm
– show
– show-diff
– status
– update-diff
} \
    -shell-script-candidates %{
        if [ "$kak_token_to_complete" -eq 0 ]; then
            printf '%s\n' add blame checkout commit diff hide-blame hide-diff init log rm show show-diff status update-diff
        else
            case "$1" in
                commit) printf -- '--%s\n' all amend fixup no-edit reset-author squash; git ls-files --modified ;;
                add) git ls-files -dmo --exclude-standard ;;
                rm) git ls-files -c ;;
            esac
        fi
    } \
    git %{
        evaluate-commands %sh{
            show_git_cmd_output() {
                local filetype
                case "$1" in
                    show|diff) filetype=diff ;;
                    log) filetype=git-log ;;
                    status) filetype=git-status ;;
                esac
                output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-git.XXXXXXXX)/fifo
                mkfifo ${output}
                ( git "$@" > ${output} 2>&1 ) < /dev/null > /dev/null 2>&1 &
                printf "
                    evaluate-commands -try-client '$kak_opt_docsclient' %%{
                        edit! -fifo $output *git*
                        set-option buffer filetype $filetype
                        hook -always -once buffer BufCloseFifo '' %%{ nop %%sh{ rm -Rf $(dirname $output) } }
                    }
                "
            }

            run_git_blame() {
                (
                    printf "
                        evaluate-commands -client $kak_client %%{
                            try %%{ add-highlighter window/git-blame flag-lines Information git_blame_flags }
                            set-option buffer=$kak_bufname git_blame_flags $kak_timestamp
                        }
                    " |
                    kak -p "$kak_session"
                    git blame "$@" --incremental "$kak_buffile" |
                    awk '
                        function send_flags(text, flag, i) {
                            if (line == "") { return; }
                            text=substr(sha, 1, 8) " " dates[sha] " " authors[sha]
                            # gsub("|", "\\|", text)
                            gsub("~", "~~", text)
                            flag="%~" line "|" text "~"
                            for (i = 1; i < count; i++) {
                                flag = flag " %~" line + i "|" text "~"
                            }
                            cmd = "kak -p " ENVIRON["kak_session"]
                            print "set-option -add buffer=" ENVIRON["kak_bufname"] " git_blame_flags " flag | cmd
                            close(cmd)
                        }
                        /^([0-9a-f]+) ([0-9]+) ([0-9]+) ([0-9]+)/ {
                            send_flags()
                            sha = $1
                            line = $3
                            count = $4
                        }
                        /^author / {
                            authors[sha] = substr($0, 8)
                        }
                        /^author-time ([0-9]*)/ {
                            cmd = "date -d @" $2 " +\"%F %T\""
                            cmd | getline dates[sha]
                            close(cmd)
                        }
                        END { send_flags(); }
                    '
                ) < /dev/null > /dev/null 2>&1 &
            }

            update_diff() {
                git --no-pager diff --unified=0 "$kak_buffile" |
                perl -e '
                    $flags = $ENV{"kak_timestamp"};
                    foreach $line (<STDIN>) {
                        if ($line =~ /@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))?/) {
                            $from_line = $1;
                            $from_count = ($2 eq "" ? 1 : $2);
                            $to_line = $3;
                            $to_count = ($4 eq "" ? 1 : $4);

                            if ($from_count == 0 and $to_count > 0) {
                                for $count (0 .. $to_count - 1) {
                                    $line = $to_line + $count;
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
                                for $count (0 .. $to_count - 1) {
                                    $line = $to_line + $count;
                                    $flags .= " $line|\{blue\}~";
                                }
                            }
                            elsif ($from_count > 0 and $from_count < $to_count) {
                                for $count (0 .. $from_count - 1) {
                                    $line = $to_line + $count;
                                    $flags .= " $line|\{blue\}~";
                                }
                                for $count ($from_count .. $to_count - 1) {
                                    $line = $to_line + $count;
                                    flags .= " $line|\{green\}+";
                                }
                            }
                            elsif ($to_count > 0 and $from_count > $to_count) {
                                for $count (0 .. $to_count - 2) {
                                    $line = $to_line + $count;
                                    flags .= " $line|\{blue\}~";
                                }
                                $last = $to_line + $to_count - 1;
                                $flags .= " $last|\{blue+u\}~";
                            }
                        }
                    }
                    print "set-option buffer git_diff_flags $flags"
                '
            }

            commit() {
                # Handle case where message needs not to be edited
                printf '%s' "$*" |
                if grep -E -q -e "-m|-F|-C|--message=.*|--file=.*|--reuse-message=.*|--no-edit|--fixup.*|--squash.*"; then
                    if git commit "$@" > /dev/null 2>&1; then
                        printf 'echo -markup {Information}Commit succeeded'
                    else
                        printf 'echo -markup {Error}Commit failed'
                    fi
                    exit
                fi
                # Fails, and generate COMMIT_EDITMSG
                GIT_EDITOR='' EDITOR='' git commit "$@" > /dev/null 2>&1
                msgfile="$(git rev-parse --git-dir)/COMMIT_EDITMSG"
                printf "
                    edit $msgfile
                    hook buffer BufWritePost '.*\Q$msgfile\E' %%{
                        evaluate-commands %%sh{
                            if git commit --file $msgfile --cleanup=strip $* > /dev/null; then
                                printf 'evaluate-commands -client $kak_client echo -markup {Information}Commit succeeded; delete-buffer'
                            else
                                printf 'evaluate-commands -client $kak_client echo -markup {Error}Commit failed'
                            fi
                        }
                    }
                "
            }

            case "$1" in
                show|log|diff|status) show_git_cmd_output "$@" ;;
                blame) shift; run_git_blame "$@" ;;
                hide-blame)
                    printf "
                        try %%{
                            set-option buffer=$kak_bufname git_blame_flags $kak_timestamp
                            remove-highlighter window/git-blame
                        }
                    "
                    ;;
                show-diff)
                    printf 'try %{ add-highlighter window/git-diff flag-lines Default git_diff_flags }'
                    update_diff
                    ;;
                hide-diff)
                    printf 'try %{ remove-highlighter window/git-diff }'
                    ;;
                update-diff) update_diff ;;
                commit) shift; commit "$@" ;;
                init) shift; git init "$@" > /dev/null 2>&1 ;;
                checkout)
                    name=${2:-$kak_buffile}
                    git checkout -- "$name" > /dev/null 2>&1
                    ;;
                add)
                    name=${2:-$kak_buffile}
                    if git add -- "$name" > /dev/null 2>&1; then
                        printf "echo -markup {Information}Git: Added $name"
                    else
                        printf "echo -markup {Error}Git: Unable to add $name"
                    fi
                    ;;
                add)
                    name=${2:-$kak_buffile}
                    if git rm -- "$name" > /dev/null 2>&1; then
                        printf "echo -markup {Information}Git: Removed $name"
                    else
                        printf "echo -markup {Error}Git: Unable to remove $name"
                    fi
                    ;;
                *)
                    printf "echo -markup {Error}Unknown Git command: $name"
            esac
        }
    }
