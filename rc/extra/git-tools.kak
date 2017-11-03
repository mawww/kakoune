declare-option -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

hook -group git-log-highlight global WinSetOption filetype=git-log %{
    add-highlighter window group git-log-highlight
    add-highlighter window/git-log-highlight regex '^(commit) ([0-9a-f]+)$' 1:yellow 2:red
    add-highlighter window/git-log-highlight regex '^([a-zA-Z_-]+:) (.*?)$' 1:green 2:magenta
    add-highlighter window/git-log-highlight ref diff # highlight potential diffs from the -p option
}

hook -group git-log-highlight global WinSetOption filetype=(?!git-log).* %{ remove-highlighter window/git-log-highlight }

hook -group git-status-highlight global WinSetOption filetype=git-status %{
    add-highlighter window group git-status-highlight
    add-highlighter window/git-status-highlight regex '^\h+(?:((?:both )?modified:)|(added:|new file:)|(deleted(?: by \w+)?:)|(renamed:)|(copied:))(?:.*?)$' 1:yellow 2:green 3:red 4:cyan 5:blue 6:magenta
}

hook -group git-status-highlight global WinSetOption filetype=(?!git-status).* %{ remove-highlighter window/git-status-highlight }

declare-option -hidden line-specs git_blame_flags
declare-option -hidden line-specs git_diff_flags

set-face GitBlame default,magenta
set-face GitDiffFlags default,black

define-command -params 1.. \
  -docstring %sh{printf '%%{git [<arguments>]: git wrapping helper
All the optional arguments are forwarded to the git utility
Available commands:\n-add\n-rm\n-blame\n-commit\n-checkout\n-diff\n-hide-blame\n-log\n-show\n-show-diff\n-status\n-update-diff}'} \
  -shell-candidates %{
    [ $kak_token_to_complete -eq 0 ] &&
        printf "add\nrm\nblame\ncommit\ncheckout\ndiff\nhide-blame\nlog\nshow\nshow-diff\nstatus\nupdate-diff\n"
  } \
  git %{ %sh{
    show_git_cmd_output() {
        local filetype
        case "$1" in
           show|diff) filetype=diff ;;
           log)  filetype=git-log ;;
           status)  filetype=git-status ;;
        esac
        output=$(mktemp -d "${TMPDIR:-/tmp}"/kak-git.XXXXXXXX)/fifo
        mkfifo ${output}
        ( git "$@" > ${output} 2>&1 ) > /dev/null 2>&1 < /dev/null &

        printf %s "evaluate-commands -try-client '$kak_opt_docsclient' %{
                  edit! -fifo ${output} *git*
                  set-option buffer filetype '${filetype}'
                  hook -group fifo buffer BufCloseFifo .* %{
                      nop %sh{ rm -r $(dirname ${output}) }
                      remove-hooks buffer fifo
                  }
              }"
    }

    run_git_blame() {
        (
            printf %s "evaluate-commands -client '$kak_client' %{
                      try %{ add-highlighter window flag_lines GitBlame git_blame_flags }
                      set-option buffer=$kak_bufname git_blame_flags '$kak_timestamp'
                  }" | kak -p ${kak_session}
                  git blame "$@" --incremental ${kak_buffile} | awk '
                  function send_flags(text, flag, i) {
                      if (line == "") { return; }
                      text=substr(sha,1,8) " " dates[sha] " " authors[sha]
                      gsub(":", "\\:", text)
                      # gsub("|", "\\|", text)
                      flag=line "|" text
                      for ( i=1; i < count; i++ ) {
                          flag=flag ":" line+i "|" text
                      }
                      cmd = "kak -p " ENVIRON["kak_session"]
                      print "set-option -add buffer=" ENVIRON["kak_bufname"] " git_blame_flags %{" flag "}" | cmd
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
        git diff -U0 $kak_buffile | awk '
            BEGIN {
                line=0
                flags=ENVIRON["kak_timestamp"]
            }
            /^---.*/ {}
            /^@@ -[0-9]+(,[0-9]+)? \+[0-9]+(,[0-9]+)? @@.*/ {
                 if ((x=index($3, ",")) > 0) {
                     line=substr($3, 2, x-2)
                 } else {
                     line=substr($3, 2)
                 }
            }
            /^\+/ {
                 flags=flags ":" line "|{green}+"
                 line++
            }
            /^\-/ { flags=flags ":" line "|{red}-" }
            END { print "set-option buffer git_diff_flags ", flags }
        '
    }

    commit() {
        # Handle case where message needs not to be edited
        if grep -E -q -e "-m|-F|-C|--message=.*|--file=.*|--reuse-message=.*|--no-edit"; then
            if git commit "$@" > /dev/null 2>&1; then
                echo 'echo -markup "{Information}Commit succeeded"'
            else
                echo 'echo -markup "{Error}Commit failed"'
            fi
            exit
        fi <<-EOF
			$@
		EOF

        # fails, and generate COMMIT_EDITMSG
        GIT_EDITOR='' EDITOR='' git commit "$@" > /dev/null 2>&1
        msgfile="$(git rev-parse --git-dir)/COMMIT_EDITMSG"
        printf %s "edit '$msgfile'
              hook buffer BufWritePost '.*\Q$msgfile\E' %{ %sh{
                  if git commit -F '$msgfile' --cleanup=strip $@ > /dev/null; then
                     printf %s 'evaluate-commands -client $kak_client echo -markup %{{Information}Commit succeeded}; delete-buffer'
                  else
                     printf %s 'evaluate-commands -client $kak_client echo -markup %{{Error}Commit failed}'
                  fi
              } }"
    }

    case "$1" in
       show|log|diff|status) show_git_cmd_output "$@" ;;
       blame) shift; run_git_blame "$@" ;;
       hide-blame)
            printf %s "try %{
                set-option buffer=$kak_bufname git_blame_flags ''
                remove-highlighter window/hlflags_git_blame_flags
            }"
            ;;
       show-diff)
           echo 'try %{ add-highlighter window flag_lines GitDiffFlags git_diff_flags }'
           update_diff
           ;;
       update-diff) update_diff ;;
       commit) shift; commit "$@" ;;
       checkout)
           name="${2:-${kak_buffile}}"
           git checkout "${name}" > /dev/null 2>&1
           ;;
       add)
           name="${2:-${kak_buffile}}"
           if git add -- "${name}" > /dev/null 2>&1; then
              printf %s "echo -markup '{Information}git: added ${name}'"
           else
              printf %s "echo -markup '{Error}git: unable to add ${name}'"
           fi
           ;;
       rm)
           name="${2:-${kak_buffile}}"
           if git rm -- "${name}" > /dev/null 2>&1; then
              printf %s "echo -markup '{Information}git: removed ${name}'"
           else
              printf %s "echo -markup '{Error}git: unable to remove ${name}'"
           fi
           ;;
       *) printf %s "echo -markup %{{Error}unknown git command '$1'}"; exit ;;
    esac
}}
