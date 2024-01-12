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
    require-module diff
    add-highlighter window/git-log group
    add-highlighter window/git-log/ regex '^([*|\\ /_.-])*' 0:keyword
    add-highlighter window/git-log/ regex '^( ?[*|\\ /_.-])*\h{,3}(commit )?(\b[0-9a-f]{4,40}\b)' 2:keyword 3:comment
    add-highlighter window/git-log/ regex '^( ?[*|\\ /_.-])*\h{,3}([a-zA-Z_-]+:) (.*?)$' 2:variable 3:value
    add-highlighter window/git-log/ ref diff # highlight potential diffs from the -p option

    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-log }
}

hook -group git-status-highlight global WinSetOption filetype=git-status %{
    require-module diff
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
    require-module diff
    add-highlighter window/git-show-branch group
    add-highlighter window/git-show-branch/ regex '(\*)|(\+)|(!)' 1:red 2:green 3:green
    add-highlighter window/git-show-branch/ regex '(!\D+\{0\}\])|(!\D+\{1\}\])|(!\D+\{2\}\])|(!\D+\{3\}\])' 1:red 2:green 3:yellow 4:blue
    add-highlighter window/git-show-branch/ regex '(\B\+\D+\{0\}\])|(\B\+\D+\{1\}\])|(\B\+\D+\{2\}\])|(\B\+\D+\{3\}\])|(\B\+\D+\{1\}\^\])' 1:red 2:green 3:yellow 4:blue 5:magenta

    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/git-show-branch}
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
            apply (alias for "patch git apply")
            blame
            checkout
            commit
            diff
            edit
            grep
            hide-blame
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
            checkout \
            commit \
            diff \
            edit \
            grep \
            hide-blame \
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
            apply) printf -- "--reverse\n--cached\n--index\n" ;;
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

    show_git_cmd_output() {
        local filetype
        local map_diff_goto_source

        case "$1" in
           diff) map_diff_goto_source=true; filetype=diff ;;
           show) map_diff_goto_source=true; filetype=git-log ;;
           show-branch) filetype=git-show-branch ;;
           log)  map_diff_goto_source=true; filetype=git-log ;;
           status)  filetype=git-status ;;
           *) return 1 ;;
        esac
        output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-git.XXXXXXXX)/fifo
        mkfifo ${output}
        ( git "$@" > ${output} 2>&1 & ) > /dev/null 2>&1 < /dev/null

        # We need to unmap in case an existing buffer changes type,
        # for example if the user runs "git show" and "git status".
        map_diff_goto_source=$([ -n "${map_diff_goto_source}" ] \
          && printf %s "map buffer normal <ret> :git-diff-goto-source<ret> -docstring 'Jump to source from git diff'" \
          || printf %s "unmap buffer normal <ret> :git-diff-goto-source<ret>")

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
                  git blame "$@" --incremental ${kak_buffile} | perl -wne '
                  use POSIX qw(strftime);
                  sub send_flags {
                      my $flush = shift;
                      if (not defined $line) { return; }
                      my $text = substr($sha,0,7) . " " . $dates{$sha} . " " . $authors{$sha};
                      $text =~ s/~/~~/g;
                      for ( my $i = 0; $i < $count; $i++ ) {
                          $flags .= " %~" . ($line+$i) . "|$text~";
                      }
                      $now = time();
                      # Send roughly one update per second, to avoid creating too many kak processes.
                      if (!$flush && defined $last_sent && $now - $last_sent < 1) {
                          return
                      }
                      open CMD, "|-", "kak -p $ENV{kak_session}";
                      print CMD "set-option -add buffer=$ENV{kak_bufname} git_blame_flags $flags";
                      close(CMD);
                      $flags = "";
                      $last_sent = $now;
                  }
                  if (m/^([0-9a-f]+) ([0-9]+) ([0-9]+) ([0-9]+)/) {
                      send_flags(0);
                      $sha = $1;
                      $line = $3;
                      $count = $4;
                  }
                  if (m/^author /) { $authors{$sha} = substr($_,7) }
                  if (m/^author-time ([0-9]*)/) { $dates{$sha} = strftime("%F %T", localtime $1) }
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
