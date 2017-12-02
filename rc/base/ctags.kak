# Kakoune Exuberant CTags support script

declare-option -docstring "colon separated list of paths to tag files to parse when looking up a symbol" \
    str-list ctagsfiles 'tags'

%sh{
    for dep in readtags ctags; do
        if ! command -v "${dep}" >/dev/null; then
            printf 'echo -debug ctags: warning, command dependency unmet: %s\n' "${dep}"
        fi
    done
}

define-command -params ..1 \
    -shell-candidates '
        printf %s\\n "$kak_opt_ctagsfiles" | tr \':\' \'\n\' |
        while read -r candidate; do
            [ -f "$candidate" ] && readlink -f "$candidate"
        done | awk \'!x[$0]++\' | # remove duplicates
        while read -r tags; do
            namecache="${tags%/*}/.kak.${tags##*/}.namecache"
            if [ -z "$(find "$namecache" -prune -newer "$tags")" ]; then
                cut -f 1 "$tags" | uniq > "$namecache"
            fi
            cat "$namecache"
        done' \
    -docstring %{ctags-search [<symbol>]: jump to a symbol's definition
If no symbol is passed then the current selection is used as symbol name} \
    ctags-search \
    %{ %sh{
        export tagname=${1:-${kak_selection}}
        printf %s\\n "$kak_opt_ctagsfiles" | tr ':' '\n' |
        while read -r candidate; do
            [ -f "$candidate" ] && readlink -f "$candidate"
        done | awk '!x[$0]++' | # remove duplicates
        while read -r tags; do
            printf '!TAGROOT\t%s\n' "$(readlink -f "${tags%/*}")/"
            readtags -t "$tags" $tagname
        done | awk -F '\t|\n' '
        /^!TAGROOT\t/ { tagroot=$2 }
        /[^\t]+\t[^\t]+\t\/\^.*\$?\// {
            re=$0;
            sub(".*\t/\\^", "", re); sub("\\$?/$", "", re); gsub("(\\{|\\}|\\\\E).*$", "", re);
            keys=re; gsub(/</, "<lt>", keys); gsub(/\t/, "<c-v><c-i>", keys);
            out = out " %{" $2 " {MenuInfo}" re "} %{evaluate-commands -collapse-jumps %{ try %{ edit %{" tagroot $2 "}; execute-keys %{/\\Q" keys "<ret>vc} } catch %{ echo %{unable to find tag} } } }"
        }
        /[^\t]+\t[^\t]+\t[0-9]+/ { out = out " %{" $2 ":" $3 "} %{evaluate-commands -collapse-jumps %{ edit %{" tagroot $2 "} %{" $3 "}}}" }
        END { print ( length(out) == 0 ? "echo -markup %{{Error}no such tag " ENVIRON["tagname"] "}" : "menu -markup -auto-single " out ) }'
    }}

define-command ctags-complete -docstring "Insert completion candidates for the current selection into the buffer's local variables" %{ evaluate-commands -draft %{
    execute-keys <space>hb<a-k>^\w+$<ret>
    %sh{ {
        compl=$(readtags -p "$kak_selection" | cut -f 1 | sort | uniq | sed -e 's/:/\\:/g' | sed -e 's/\n/:/g' )
        compl="${kak_cursor_line}.${kak_cursor_column}+${#kak_selection}@${kak_timestamp}:${compl}"
        printf %s\\n "set-option buffer=$kak_bufname ctags_completions '${compl}'" | kak -p ${kak_session}
    } > /dev/null 2>&1 < /dev/null & }
}}

define-command ctags-funcinfo -docstring "Display ctags information about a selected function" %{
    evaluate-commands -draft %{
        try %{
            execute-keys -no-hooks '[(;B<a-k>[a-zA-Z_]+\(<ret><a-;>'
            %sh{
                sigs=$(readtags -e ${kak_selection%(} | grep kind:f | sed -re 's/^(\S+).*((class|struct|namespace):(\S+))?.*signature:(.*)$/\5 [\4::\1]/')
                if [ -n "$sigs" ]; then
                    printf %s\\n "evaluate-commands -client ${kak_client} %{info -anchor $kak_cursor_line.$kak_cursor_column -placement above '$sigs'}"
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

declare-option -docstring "options to pass to the `ctags` shell command" \
    str ctagsopts "-R"
declare-option -docstring "path to the directory in which the tags file will be generated" str ctagspaths "."

define-command ctags-generate -docstring 'Generate tag file asynchronously' %{
    echo -markup "{Information}launching tag generation in the background"
    %sh{ {
        while ! mkdir .tags.kaklock 2>/dev/null; do sleep 1; done
        trap 'rmdir .tags.kaklock' EXIT

        if ctags -f .tags.kaktmp ${kak_opt_ctagsopts} ${kak_opt_ctagspaths}; then
            mv .tags.kaktmp tags
            msg="tags generation complete"
        else
            msg="tags generation failed"
        fi

        printf %s\\n "evaluate-commands -client $kak_client echo -markup '{Information}${msg}'" | kak -p ${kak_session}
    } > /dev/null 2>&1 < /dev/null & }
}

define-command ctags-update-tags -docstring 'Update tags for the given file' %{
    %sh{ {
        while ! mkdir .tags.kaklock 2>/dev/null; do sleep 1; done
            trap 'rmdir .tags.kaklock' EXIT

        if ctags -f .file_tags.kaktmp ${kak_opt_ctagsopts} $kak_bufname; then
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
    } > /dev/null 2>&1 < /dev/null & }
}
