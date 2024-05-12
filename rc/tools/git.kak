declare-option -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

declare-option -docstring "git diff added character" \
    str git_diff_add_char "▏"

declare-option -docstring "git diff modified character" \
    str git_diff_mod_char "▏"

declare-option -docstring "git diff deleted character" \
    str git_diff_del_char "_"

declare-option -docstring "git diff top deleted character" \
    str git_diff_top_char "‾"

hook -group git-log-highlight global WinSetOption filetype=git-log %{
    add-highlighter window/git-log group
    add-highlighter window/git-log/ regex '^([*|\\ /_.-])*' 0:keyword
    add-highlighter window/git-log/ regex '^( ?[*|\\ /_.-])*\h{,3}(commit )?(\b[0-9a-f]{4,40}\b)' 2:keyword 3:comment
    add-highlighter window/git-log/ regex '^( ?[*|\\ /_.-])*\h{,3}([a-zA-Z_-]+:) (.*?)$' 2:variable 3:value
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-log }
}

hook global WinSetOption filetype=diff %{
    try %{
        execute-keys -draft %{/^diff --git\b<ret>}
        evaluate-commands %sh{
            if [ -n "$(git ls-files -- "${kak_buffile}")" ]; then
                echo fail
            fi
        }
        set-option buffer filetype git-diff
    }
}

hook -group git-diff-highlight global WinSetOption filetype=(git-diff|git-log) %{
    require-module diff
    add-highlighter %exp{window/%val{hook_param_capture_1}-ref-diff} ref diff
    hook -once -always window WinSetOption filetype=.* %exp{
        remove-highlighter window/%val{hook_param_capture_1}-ref-diff
    }
}

hook global WinSetOption filetype=(?:git-diff|git-log) %{
    map buffer normal <ret> %exp{:git-diff-goto-source # %val{hook_param}<ret>} -docstring 'Jump to source from git diff'
    hook -once -always window WinSetOption filetype=.* %exp{
        unmap buffer normal <ret> %%{:git-diff-goto-source # %val{hook_param}<ret>}
    }
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

hook -group git-show-branch-highlight global WinSetOption filetype=git-show-branch %{
    add-highlighter window/git-show-branch group
    add-highlighter window/git-show-branch/ regex '(\*)|(\+)|(!)' 1:red 2:green 3:green
    add-highlighter window/git-show-branch/ regex '(!\D+\{0\}\])|(!\D+\{1\}\])|(!\D+\{2\}\])|(!\D+\{3\}\])' 1:red 2:green 3:yellow 4:blue
    add-highlighter window/git-show-branch/ regex '(\B\+\D+\{0\}\])|(\B\+\D+\{1\}\])|(\B\+\D+\{2\}\])|(\B\+\D+\{3\}\])|(\B\+\D+\{1\}\^\])' 1:red 2:green 3:yellow 4:blue 5:magenta

    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-show-branch}
}

declare-option -hidden line-specs git_blame_flags
declare-option -hidden line-specs git_blame_index
declare-option -hidden str git_blame
declare-option -hidden str git_blob
declare-option -hidden line-specs git_diff_flags
declare-option -hidden int-list git_hunk_list

define-command -params 1.. \
    -docstring %{
        git [<arguments>]: git wrapping helper
        All the optional arguments are forwarded to the git utility
        Available commands:
            add
            apply      - alias for "patch git apply"
            blame      - toggle blame annotations
            blame-jump - show the commit that added the line at cursor
            checkout
            commit
            diff
            edit
            grep
            hide-diff
            init
            log
            next-hunk
            prev-hunk
            reset
            rm
            show
            show-branch
            show-diff
            status
            update-diff
    } -shell-script-candidates %{
    if [ $kak_token_to_complete -eq 0 ]; then
        printf %s\\n \
            apply \
            blame \
            blame-jump \
            checkout \
            commit \
            diff \
            edit \
            grep \
            hide-diff \
            init \
            log \
            next-hunk \
            prev-hunk \
            reset \
            rm \
            show \
            show-branch \
            show-diff \
            status \
            update-diff \
        ;
    else
        case "$1" in
            commit) printf -- "--amend\n--no-edit\n--all\n--reset-author\n--fixup\n--squash\n"; git ls-files -m ;;
            add) git ls-files -dmo --exclude-standard ;;
            apply) printf -- "--reverse\n--cached\n--index\n--3way\n" ;;
            grep|edit) git ls-files -c --recurse-submodules ;;
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
    kakquote() {
        printf "%s" "$1" | sed "s/'/''/g; 1s/^/'/; \$s/\$/'/"
    }

    show_git_cmd_output() {
        local filetype

        case "$1" in
           diff) filetype=git-diff ;;
           show) filetype=git-log ;;
           show-branch) filetype=git-show-branch ;;
           log)  filetype=git-log ;;
           status)  filetype=git-status ;;
           *) return 1 ;;
        esac
        output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-git.XXXXXXXX)/fifo
        mkfifo ${output}
        ( trap - INT QUIT; git "$@" > ${output} 2>&1 & ) > /dev/null 2>&1 < /dev/null

        printf %s "evaluate-commands -try-client '$kak_opt_docsclient' '
                  edit! -fifo ${output} *git*
                  set-option buffer filetype ${filetype}
                  $(hide_blame)
                  set-option buffer git_blob %{}
                  hook -always -once buffer BufCloseFifo .* ''
                      nop %sh{ rm -r $(dirname ${output}) }
                      $(printf %s "${on_close_fifo}" | sed "s/'/''''/g")
                  ''
        '"
    }

    hide_blame() {
        printf %s "
            set-option buffer git_blame_flags $kak_timestamp
            set-option buffer git_blame_index $kak_timestamp
            set-option buffer git_blame %{}
            try %{ remove-highlighter window/git-blame }
            unmap window normal <ret> %{:git blame-jump<ret>}
        "
    }

    prepare_git_blame_args='
        if [ -n "${kak_opt_git_blob}" ]; then {
            contents_fifo=/dev/null
            set -- "$@" "${kak_opt_git_blob%%:*}" -- "${kak_opt_git_blob#*:}"
        } else {
            contents_fifo=$(mktemp -d "${TMPDIR:-/tmp}"/kak-git.XXXXXXXX)/fifo
            mkfifo ${contents_fifo}
            echo >${kak_command_fifo} "evaluate-commands -save-regs | %{
                set-register | %{
                    contents=\$(cat; printf .)
                    ( printf %s \"\${contents%.}\" >${contents_fifo} ) >/dev/null 2>&1 &
                }
                execute-keys -client ${kak_client} -draft %{%<a-|><ret>}
            }"
            set -- "$@" --contents - -- "${kak_buffile}"
        } fi
    '

    blame_toggle() {
        echo >${kak_command_fifo} "try %{
            add-highlighter window/git-blame flag-lines Information git_blame_flags
            echo -to-file ${kak_response_fifo}
        } catch %{
            echo -to-file ${kak_response_fifo} 'hide_blame; exit'
        }"
        eval $(cat ${kak_response_fifo})
        if [ -z "${kak_opt_git_blob}" ] && {
            [ "${kak_opt_filetype}" = git-diff ] || [ "${kak_opt_filetype}" = git-log ]
        } then {
            echo 'try %{ remove-highlighter window/git-blame }'
            printf >${kak_command_fifo} %s '
                evaluate-commands -client '${kak_client}' -draft %{
                    try %{
                        execute-keys <a-l><semicolon><a-?>^commit<ret><a-semicolon>
                    } catch %{
                        # Missing commit line, assume it is an uncommitted change.
                        execute-keys <a-l><semicolon>Gg<a-semicolon>
                    }
                    require-module diff
                    try %{
                        diff-parse END %{
                            my $line = $file_line;
                            if (not defined $commit) {
                                $commit = "HEAD";
                                $line = $other_file_line;
                                if ($diff_line_text =~ m{^\+}) {
                                    print "echo -to-file '${kak_response_fifo}' -quoting shell "
                                        . "%{git blame: blame from HEAD does not work on added lines}";
                                    exit;
                                }
                            } elsif ($diff_line_text =~ m{^[-]}) {
                                $commit = "$commit~";
                                $line = $other_file_line;
                            }
                            $line = $line or 1;
                            printf "echo -to-file '${kak_response_fifo}' -quoting shell %s %s %d %d",
                                $commit, quote($file), $line, ('${kak_cursor_column}' - 1);
                        }
                    } catch %{
                        echo -to-file '${kak_response_fifo}' -quoting shell -- %val{error}
                    }
                }
            '
            n=$#
            eval set -- "$(cat ${kak_response_fifo})" "$@"
            if [ $# -eq $((n+1)) ]; then
                echo fail -- "$(kakquote "$1")"
                exit
            fi
            commit=$1
            file=${2#"$PWD/"}
            cursor_line=$3
            cursor_column=$4
            shift 4
            # Log commit and file name because they are only echoed briefly
            # and not shown elsewhere (we don't have a :messages buffer).
            message="Blaming $file as of $(git rev-parse --short $commit)"
            echo "echo -debug -- $(kakquote "$message")"
            on_close_fifo="
                execute-keys -client ${kak_client} ${cursor_line}g<a-h>${cursor_column}lh
                evaluate-commands -client ${kak_client} %{
                    set-option buffer git_blob $(kakquote "$commit:$file")
                    git blame $(for arg; do kakquote "$arg"; printf " "; done)
                    hook -once window NormalIdle .* %{
                        execute-keys vv
                        echo -markup -- $(kakquote "{Information}{\\}$message. Press <ret> to jump to blamed commit")
                    }
                }
            " show_git_cmd_output show "$commit:$file"
            exit
        } fi
        eval "$prepare_git_blame_args"
        echo 'map window normal <ret> %{:git blame-jump<ret>}'
        echo 'echo -markup {Information}Press <ret> to jump to blamed commit'
        (
            trap - INT QUIT
            cd_bufdir
            printf %s "evaluate-commands -client '$kak_client' %{
                      set-option buffer=$kak_bufname git_blame_flags '$kak_timestamp'
                      set-option buffer=$kak_bufname git_blame_index '$kak_timestamp'
                      set-option buffer=$kak_bufname git_blame ''
                  }" | kak -p ${kak_session}
            if ! stderr=$({ git blame --incremental "$@" <${contents_fifo} | perl -wne '
                  use POSIX qw(strftime);
                  sub quote {
                      my $SQ = "'\''";
                      my $token = shift;
                      $token =~ s/$SQ/$SQ$SQ/g;
                      return "$SQ$token$SQ";
                  }
                  sub send_flags {
                      my $is_last_call = shift;
                      if (not defined $line) {
                          if ($is_last_call) { exit 1; }
                          return;
                      }
                      my $text = substr($sha,0,7) . " " . $dates{$sha} . " " . $authors{$sha};
                      $text =~ s/~/~~/g;
                      for ( my $i = 0; $i < $count; $i++ ) {
                          $flags .= " %~" . ($line+$i) . "|$text~";
                      }
                      $now = time();
                      # Send roughly one update per second, to avoid creating too many kak processes.
                      if (!$is_last_call && defined $last_sent && $now - $last_sent < 1) {
                          return
                      }
                      open CMD, "|-", "kak -p $ENV{kak_session}";
                      print CMD "set-option -add buffer=$ENV{kak_bufname} git_blame_flags $flags;";
                      print CMD "set-option -add buffer=$ENV{kak_bufname} git_blame_index $index;";
                      print CMD "set-option -add buffer=$ENV{kak_bufname} git_blame " . quote $raw_blame;
                      close(CMD);
                      $flags = "";
                      $index = "";
                      $raw_blame = "";
                      $last_sent = $now;
                  }
                  $raw_blame .= $_;
                  chomp;
                  if (m/^([0-9a-f]+) ([0-9]+) ([0-9]+) ([0-9]+)/) {
                      send_flags(0);
                      $sha = $1;
                      $line = $3;
                      $count = $4;
                      for ( my $i = 0; $i < $count; $i++ ) {
                          $index .= " " . ($line+$i) . "|$.,$i";
                      }
                  }
                  if (m/^author /) {
                      $authors{$sha} = substr($_,7);
                      $authors{$sha} = "Not Committed Yet" if $authors{$sha} eq "External file (--contents)";
                  }
                  if (m/^author-time ([0-9]*)/) { $dates{$sha} = strftime("%F %T", localtime $1) }
                  END { send_flags(1); }'
            } 2>&1); then
                escape2() { printf %s "$*" | sed "s/'/''''/g"; }
                echo "evaluate-commands -client ${kak_client} '
                    evaluate-commands -draft %{
                        buffer %{${kak_buffile}}
                        git hide-blame
                    }
                    echo -debug failed to run git blame
                    echo -debug git stderr: <<<
                    echo -debug ''$(escape2 "$stderr")>>>''
                    hook -once buffer NormalIdle .* %{
                        echo -markup %{{Error}failed to run git blame, see *debug* buffer}
                    }
                '" | kak -p ${kak_session}
            fi
            if [ "$contents_fifo" != /dev/null ]; then
                rm -r $(dirname $contents_fifo)
            fi
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
            git --no-pager diff --no-ext-diff -U0 "$kak_buffile" | perl -e '
            use utf8;
            $flags = $ENV{"kak_timestamp"};
            $add_char = $ENV{"kak_opt_git_diff_add_char"};
            $del_char = $ENV{"kak_opt_git_diff_del_char"};
            $top_char = $ENV{"kak_opt_git_diff_top_char"};
            $mod_char = $ENV{"kak_opt_git_diff_mod_char"};
            foreach $line (<STDIN>) {
                if ($line =~ /@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))?/) {
                    $from_line = $1;
                    $from_count = ($2 eq "" ? 1 : $2);
                    $to_line = $3;
                    $to_count = ($4 eq "" ? 1 : $4);

                    if ($from_count == 0 and $to_count > 0) {
                        for $i (0..$to_count - 1) {
                            $line = $to_line + $i;
                            $flags .= " $line|\{green\}$add_char";
                        }
                    }
                    elsif ($from_count > 0 and $to_count == 0) {
                        if ($to_line == 0) {
                            $flags .= " 1|\{red\}$top_char";
                        } else {
                            $flags .= " $to_line|\{red\}$del_char";
                        }
                    }
                    elsif ($from_count > 0 and $from_count == $to_count) {
                        for $i (0..$to_count - 1) {
                            $line = $to_line + $i;
                            $flags .= " $line|\{blue\}$mod_char";
                        }
                    }
                    elsif ($from_count > 0 and $from_count < $to_count) {
                        for $i (0..$from_count - 1) {
                            $line = $to_line + $i;
                            $flags .= " $line|\{blue\}$mod_char";
                        }
                        for $i ($from_count..$to_count - 1) {
                            $line = $to_line + $i;
                            $flags .= " $line|\{green\}$add_char";
                        }
                    }
                    elsif ($to_count > 0 and $from_count > $to_count) {
                        for $i (0..$to_count - 2) {
                            $line = $to_line + $i;
                            $flags .= " $line|\{blue\}$mod_char";
                        }
                        $last = $to_line + $to_count - 1;
                        $flags .= " $last|\{blue+u\}$mod_char";
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
            echo "fail 'no git hunks found, try \":git show-diff\" first'"
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

    blame_jump() {
        echo >${kak_command_fifo} "echo -to-file ${kak_response_fifo} -- %opt{git_blame}"
        blame_info=$(cat < ${kak_response_fifo})
        blame_index=
        cursor_column=${kak_cursor_column}
        cursor_line=${kak_cursor_line}
        if [ -n "$blame_info" ]; then {
            echo >${kak_command_fifo} "
                update-option buffer git_blame_index
                echo -to-file ${kak_response_fifo} -- %opt{git_blame_index}
            "
            blame_index=$(cat < ${kak_response_fifo})
        } elif [ "${kak_opt_filetype}" = git-diff ] || [ "${kak_opt_filetype}" = git-log ]; then {
            printf >${kak_command_fifo} %s '
                evaluate-commands -draft %{
                    try %{
                        execute-keys <a-l><semicolon><a-?>^commit<ret><a-semicolon>
                    } catch %{
                        # Missing commit line, assume it is an uncommitted change.
                        execute-keys <a-l><semicolon><a-?>\A<ret><a-semicolon>
                    }
                    require-module diff
                    try %{
                        diff-parse BEGIN %{
                            $version = "-";
                        } END %{
                            if ($diff_line_text !~ m{^[ -]}) {
                                print "set-register e fail git blame-jump: recursive blame only works on context or deleted lines";
                            } else {
                                if (not defined $commit) {
                                    $commit = "HEAD";
                                } else {
                                    $commit = "$commit~" if $diff_line_text =~ m{^[- ]};
                                }
                                printf "echo -to-file '${kak_response_fifo}' -quoting shell %s %s %d %d",
                                        $commit, quote($file), $file_line, ('$cursor_column' - 1);
                            }
                        }
                    } catch %{
                        echo -to-file '${kak_response_fifo}' -quoting shell -- %val{error}
                    }
                }
            '
            eval set -- "$(cat ${kak_response_fifo})"
            if [ $# -eq 1 ]; then
                echo fail -- "$(kakquote "$1")"
                exit
            fi
            starting_commit=$1
            file=$2
            cursor_line=$3
            cursor_column=$4
            blame_info=$(git blame --porcelain "$starting_commit" -L"$cursor_line,$cursor_line" -- "$file")
            if [ $? -ne 0 ]; then
                echo 'echo -markup %{{Error}failed to run git blame, see *debug* buffer}'
                exit
            fi
        } else {
            set --
            eval "$prepare_git_blame_args"
            blame_info=$(git blame --porcelain -L"$cursor_line,$cursor_line" "$@" <${contents_fifo})
            status=$?
            if [ "$contents_fifo" != /dev/null ]; then
                rm -r $(dirname $contents_fifo)
            fi
            if [ $status -ne 0 ]; then
                echo 'echo -markup %{{Error}failed to run git blame, see *debug* buffer}'
                exit
            fi
        } fi
        eval "$(printf '%s\n---\n%s' "$blame_index" "$blame_info" |
                client=${kak_opt_docsclient:-$kak_client} \
                cursor_line=$cursor_line cursor_column=$cursor_column \
                perl -wne '
            BEGIN {
                use POSIX qw(strftime);
                our $SQ = "'\''";
                sub escape {
                   return shift =~ s/$SQ/$SQ$SQ/gr
                }
                sub quote {
                    my $token = escape shift;
                    return "$SQ$token$SQ";
                }
                sub shellquote {
                    my $token = shift;
                    $token =~ s/$SQ/$SQ\\$SQ$SQ/g;
                    return "$SQ$token$SQ";
                }
                sub perlquote {
                    my $token = shift;
                    $token =~ s/\\/\\\\/g;
                    $token =~ s/$SQ/\\$SQ/g;
                    return "$SQ$token$SQ";
                }
                $target = $ENV{"cursor_line"};
                $state = "index";
            }
            chomp;
            if ($state eq "index") {
                if ($_ eq "---") {
                    $state = "blame";
                    next;
                }
                @blame_index = split;
                next unless @blame_index;
                shift @blame_index;
                foreach (@blame_index) {
                    $_ =~ m{(\d+)\|(\d+),(\d+)} or die "bad blame index flag: $_";
                    my $buffer_line = $1;
                    if ($buffer_line == $target) {
                        $target_in_blame = $2;
                        $target_offset = $3;
                        last;
                    }
                }
                defined $target_in_blame and next, or last;
            }
            if (m/^([0-9a-f]+) ([0-9]+) ([0-9]+) ([0-9]+)/) {
                if ($done) {
                    last;
                }
                $sha = $1;
                $old_line = $2;
                $new_line = $3;
                $count = $4;
                if (defined $target_in_blame) {
                    if ($target_in_blame == $. - 2) {
                        $old_line += $target_offset;
                        $done = 1;
                    }
                } else {
                    if ($new_line <= $target and $target < $new_line + $count) {
                        $old_line += $target - $new_line;
                        $done = 1;
                    }
                }
            }
            if (m/^filename /) { $old_filenames{$sha} = substr($_,9) }
            if (m/^author /) { $authors{$sha} = substr($_,7) }
            if (m/^author-time ([0-9]*)/) { $dates{$sha} = strftime("%F", localtime $1) }
            if (m/^summary /) { $summaries{$sha} = substr($_,8) }
            END {
                if (@blame_index and not defined $target_in_blame) {
                    print "echo fail git blame-jump: line has no blame information;";
                    exit;
                }
                if (not defined $sha) {
                    print "echo fail git blame-jump: missing blame info";
                    exit;
                }
                if (not $done) {
                    print "echo \"fail git blame-jump: line not found in annotations (blame still loading?)\"";
                    exit;
                }
                $info = "{Information}{\\}";
                if ($sha =~ m{^0+$}) {
                    $old_filename = $ENV{"kak_buffile"};
                    $old_filename = substr $old_filename, length($ENV{"PWD"}) + 1;
                    $show_diff = "diff HEAD";
                    $info .= "Not committed yet";
                } else {
                    $old_filename = $old_filenames{$sha};
                    $author = $authors{$sha};
                    $date = $dates{$sha};
                    $summary = $summaries{$sha};
                    $show_diff = "show $sha";
                    $info .= "$date $author \"$summary\"";
                }
                $on_close_fifo = "
                    evaluate-commands -draft $SQ
                        execute-keys <percent>
                        require-module diff
                        diff-parse BEGIN %{
                            \$in_file = " . escape(perlquote($old_filename)) . ";
                            \$in_file_line = $old_line;
                        } END $SQ$SQ
                            print \"execute-keys -client $ENV{client} \${diff_line}g<a-h>$ENV{cursor_column}l;\";
                            printf \"evaluate-commands -client $ENV{client} $SQ$SQ$SQ$SQ
                                hook -once window NormalIdle .* $SQ$SQ$SQ$SQ$SQ$SQ$SQ$SQ
                                    execute-keys vv
                                    echo -markup -- %s
                                $SQ$SQ$SQ$SQ$SQ$SQ$SQ$SQ
                            $SQ$SQ$SQ$SQ ;\"," . escape(escape(perlquote(escape(escape(quote($info)))))) . ";
                        $SQ$SQ
                    $SQ
                ";
                printf "on_close_fifo=%s show_git_cmd_output %s",
                    shellquote($on_close_fifo), $show_diff;
            }
        ')"
    }

    case "$1" in
        apply)
            shift
            enquoted="$(printf '"%s" ' "$@")"
            echo "require-module patch"
            echo "patch git apply $enquoted"
            ;;
        show|show-branch|log|diff|status)
            show_git_cmd_output "$@"
            ;;
        blame)
            shift
            blame_toggle "$@"
            ;;
        blame-jump)
            blame_jump
            ;;
        hide-blame)
            hide_blame
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
            run_git_cmd $cmd "${@:-"${kak_buffile}"}"
            ;;
        reset|checkout)
            run_git_cmd "$@"
            ;;
        grep)
            shift
            enquoted="$(printf '"%s" ' "$@")"
            printf %s "try %{
                set-option current grepcmd 'git grep -n --column'
                grep $enquoted
                set-option current grepcmd '$kak_opt_grepcmd'
            }"
            ;;
        edit)
            shift
            enquoted="$(printf '"%s" ' "$@")"
            printf %s "edit -existing -- $enquoted"
            ;;
        *)
            printf "fail unknown git command '%s'\n" "$1"
            exit
            ;;
    esac
}}

# Works within :git diff and :git show
define-command git-diff-goto-source \
    -docstring 'Navigate to source by pressing the enter key in hunks when git diff is displayed. Works within :git diff and :git show' %{
    require-module diff
    diff-jump %sh{ git rev-parse --show-toplevel }
}
