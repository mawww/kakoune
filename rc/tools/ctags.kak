# Kakoune CTags support script
#
# This script requires the readtags command available in universal-ctags

declare-option -docstring "minimum characters before triggering autocomplete" \
    int ctags_min_chars 3

declare-option -docstring "list of paths to tag files to parse when looking up a symbol" \
    str-list ctagsfiles 'tags'

declare-option -hidden completions ctags_completions

declare-option -docstring "shell command to run" str readtagscmd "readtags"

define-command -params ..1 \
    -shell-script-candidates %{
        realpath() { ( cd "$(dirname "$1")"; printf "%s/%s\n" "$(pwd -P)" "$(basename "$1")" ) }
        eval "set -- $kak_quoted_opt_ctagsfiles"
        for candidate in "$@"; do
            [ -f "$candidate" ] && realpath "$candidate"
        done | awk '!x[$0]++' | # remove duplicates
        while read -r tags; do
            namecache="${tags%/*}/.kak.${tags##*/}.namecache"
            if [ -z "$(find "$namecache" -prune -newer "$tags")" ]; then
                cut -f 1 "$tags" | grep -v '^!' | uniq > "$namecache"
            fi
            cat "$namecache"
        done} \
    -docstring %{
        ctags-search [<symbol>]: jump to a symbol's definition
        If no symbol is passed then the current selection is used as symbol name
    } \
    ctags-search %[ evaluate-commands %sh[
        realpath() { ( cd "$(dirname "$1")"; printf "%s/%s\n" "$(pwd -P)" "$(basename "$1")" ) }
        export tagname="${1:-${kak_selection}}"
        eval "set -- $kak_quoted_opt_ctagsfiles"
        for candidate in "$@"; do
            [ -f "$candidate" ] && realpath "$candidate"
        done | awk '!x[$0]++' | # remove duplicates
        while read -r tags; do
            printf '!TAGROOT\t%s\n' "$(realpath "${tags%/*}")/"
            ${kak_opt_readtagscmd} -t "$tags" "$tagname"
        done | awk -F '\t|\n' '
            /^!TAGROOT\t/ { tagroot=$2 }
            /[^\t]+\t[^\t]+\t\/\^.*\$?\// {
                line = $0; sub(".*\t/\\^", "", line); sub("\\$?/$", "", line);
                menu_info = line; gsub("!", "!!", menu_info); gsub(/^[\t ]+/, "", menu_info); gsub(/\t/, " ", menu_info);
                keys = line; gsub(/</, "<lt>", keys); gsub(/\t/, "<c-v><c-i>", keys); gsub("!", "!!", keys); gsub("&", "&&", keys); gsub("#", "##", keys); gsub("\\|", "||", keys); gsub("\\\\/", "/", keys);
                menu_item = $2; gsub("!", "!!", menu_item);
                edit_path = path($2); gsub("&", "&&", edit_path); gsub("#", "##", edit_path); gsub("\\|", "||", edit_path);
                select = $1; gsub(/</, "<lt>", select); gsub(/\t/, "<c-v><c-i>", select); gsub("!", "!!", select); gsub("&", "&&", select); gsub("#", "##", select); gsub("\\|", "||", select);
                out = out "%!" menu_item ": {MenuInfo}{\\}" menu_info "! %!evaluate-commands %# try %& edit -existing %|" edit_path "|; execute-keys %|/\\Q" keys "<ret>vc| & catch %& fail unable to find tag &; try %& execute-keys %|s\\Q" select "<ret>| & # !"
            }
            /[^\t]+\t[^\t]+\t[0-9]+/ {
                menu_item = $2; gsub("!", "!!", menu_item);
                select = $1; gsub(/</, "<lt>", select); gsub(/\t/, "<c-v><c-i>", select); gsub("!", "!!", select); gsub("&", "&&", select); gsub("#", "##", select); gsub("\\|", "||", select);
                menu_info = $3; gsub("!", "!!", menu_info);
                edit_path = path($2); gsub("!", "!!", edit_path); gsub("#", "##", edit_path); gsub("&", "&&", edit_path); gsub("\\|", "||", edit_path);
                line_number = $3;
                out = out "%!" menu_item ": {MenuInfo}{\\}" menu_info "! %!evaluate-commands %# try %& edit -existing %|" edit_path "|; execute-keys %|" line_number "gx| & catch %& fail unable to find tag &; try %& execute-keys %|s\\Q" select "<ret>| & # !"
            }
            END { print ( length(out) == 0 ? "fail no such tag " ENVIRON["tagname"] : "menu -markup -auto-single " out ) }
            # Ensure x is an absolute file path, by prepending with tagroot
            function path(x) { return x ~/^\// ? x : tagroot x }'
    ]]

define-command ctags-complete -docstring "Complete the current selection" %{
    nop %sh{
        (
            header="${kak_cursor_line}.${kak_cursor_column}@${kak_timestamp}"
            compl=$(
                eval "set -- $kak_quoted_opt_ctagsfiles"
                for ctagsfile in "$@"; do
                    ${kak_opt_readtagscmd} -p -t "$ctagsfile" ${kak_selection}
                done | awk '{ uniq[$1]++ } END { for (elem in uniq) printf " %1$s||%1$s", elem }'
            )
            printf %s\\n "evaluate-commands -client ${kak_client} set-option buffer=${kak_bufname} ctags_completions ${header}${compl}" | \
                kak -p ${kak_session}
        ) > /dev/null 2>&1 < /dev/null &
    }
}

define-command ctags-funcinfo -docstring "Display ctags information about a selected function" %{
    evaluate-commands -draft %{
        try %{
            execute-keys '[(;B<a-k>[a-zA-Z_]+\(<ret><a-;>'
            evaluate-commands %sh{
                f=${kak_selection%?}
                sig='\tsignature:(.*)'
                csn='\t(class|struct|namespace):(\S+)'
                sigs=$(${kak_opt_readtagscmd} -e -Q '(eq? $kind "f")' "${f}" | sed -Ee "s/^.*${csn}.*${sig}$/\3 [\2::${f}]/ ;t ;s/^.*${sig}$/\1 [${f}]/")
                if [ -n "$sigs" ]; then
                    printf %s\\n "evaluate-commands -client ${kak_client} %{info -anchor $kak_cursor_line.$kak_cursor_column -style above '$sigs'}"
                fi
            }
        }
    }
}

define-command ctags-enable-autoinfo -docstring "Automatically display ctags information about function" %{
     hook window -group ctags-autoinfo NormalIdle .* ctags-funcinfo
     hook window -group ctags-autoinfo InsertIdle .* ctags-funcinfo
}

define-command ctags-disable-autoinfo -docstring "Disable automatic ctags information displaying" %{ remove-hooks window ctags-autoinfo }

declare-option -docstring "shell command to run" \
    str ctagscmd "ctags -R --fields=+S"
declare-option -docstring "path to the directory in which the tags file will be generated" str ctagspaths "."

define-command ctags-generate -docstring 'Generate tag file asynchronously' %{
    echo -markup "{Information}launching tag generation in the background"
    nop %sh{ (
        while ! mkdir .tags.kaklock 2>/dev/null; do sleep 1; done
        trap 'rmdir .tags.kaklock' EXIT

        if ${kak_opt_ctagscmd} -f .tags.kaktmp ${kak_opt_ctagspaths}; then
            mv .tags.kaktmp tags
            msg="tags generation complete"
        else
            msg="tags generation failed"
        fi

        printf %s\\n "evaluate-commands -client $kak_client echo -markup '{Information}${msg}'" | kak -p ${kak_session}
    ) > /dev/null 2>&1 < /dev/null & }
}

define-command ctags-update-tags -docstring 'Update tags for the given file' %{
    nop %sh{ (
        while ! mkdir .tags.kaklock 2>/dev/null; do sleep 1; done
            trap 'rmdir .tags.kaklock' EXIT

        if ${kak_opt_ctagscmd} -f .file_tags.kaktmp $kak_bufname; then
            export LC_COLLATE=C LC_ALL=C # ensure ASCII sorting order
            # merge the updated tags tags with the general tags (filtering out out of date tags from it) into the target file
            grep -Fv "$(printf '\t%s\t' "$kak_bufname")" tags | grep -v '^!' | sort --merge - .file_tags.kaktmp >> .tags.kaktmp
            rm .file_tags.kaktmp
            mv .tags.kaktmp tags
            msg="tags updated for $kak_bufname"
        else
            msg="tags update failed for $kak_bufname"
        fi

        printf %s\\n "evaluate-commands -client $kak_client echo -markup '{Information}${msg}'" | kak -p ${kak_session}
    ) > /dev/null 2>&1 < /dev/null & }
}

define-command ctags-enable-autocomplete -docstring "Enable automatic ctags completion" %{
    set-option window completers "option=ctags_completions" %opt{completers}
    hook window -group ctags-autocomplete InsertIdle .* %{
        try %{
            evaluate-commands -draft %{ # select previous word >= ctags_min_chars
                execute-keys ",b_<a-k>.{%opt{ctags_min_chars},}<ret>"
                ctags-complete          # run in draft context to preserve selection
            }
        }
    }
}

define-command ctags-disable-autocomplete -docstring "Disable automatic ctags completion" %{
    evaluate-commands %sh{
        printf "set-option window completers %s\n" $(printf %s "${kak_opt_completers}" | sed -e "s/'option=ctags_completions'//g")
    }
    remove-hooks window ctags-autocomplete
}
