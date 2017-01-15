decl -hidden range-faces spell_regions
decl -hidden str spell_tmp_file

def -params ..1 \
    -docstring %{spell [<language>]: spell check the current buffer
The first optional argument is the language against which the check will be performed
Formats of language supported:
 - ISO language code, e.g. 'en'
 - language code above followed by a dash or underscore with an ISO country code, e.g. 'en-US'} \
    spell %{
    try %{ add-highlighter ranges 'spell_regions' }
    %sh{
        file=$(mktemp -d -t kak-spell.XXXXXXXX)/buffer
        printf 'eval -no-hooks write %s\n' "${file}"
        printf 'set buffer spell_tmp_file %s\n' "${file}"
    }
    %sh{
        if [ $# -ge 1 ]; then
            if [ ${#1} -ne 2 -a ${#1} -ne 5 ]; then
                echo 'echo -color Error Invalid language code (examples of expected format: en, en_US, en-US)'
                rm -r "$(dirname "$kak_opt_spell_tmp_file")"
                exit 1
            else
                options="-l '$1'"
            fi
        fi

        {
            sed 's/^/^/' "$kak_opt_spell_tmp_file" | eval "aspell --byte-offsets -a $options" 2>&1 | {
                line_num=1
                regions=$kak_timestamp
                read line # drop the identification message
                while read -r line; do
                    case "$line" in
                        [\#\&]*)
                            if expr "$line" : '^&' >/dev/null; then
                               pos=$(printf %s\\n "$line" | cut -d ' ' -f 4 | sed 's/:$//')
                            else
                               pos=$(printf %s\\n "$line" | cut -d ' ' -f 3)
                            fi
                            word=$(printf %s\\n "$line" | cut -d ' ' -f 2)
                            len=$(printf %s "$word" | wc -c)
                            regions="$regions:$line_num.$pos+${len}|Error"
                            ;;
                        '') line_num=$((line_num + 1));;
                        \*) ;;
                        *) printf 'echo -color Error %%{%s}\n' "${line}" | kak -p "${kak_session}";;
                    esac
                done
                printf 'set "buffer=%s" spell_regions %%{%s}' "${kak_bufname}" "${regions}" \
                    | kak -p "${kak_session}"
            }
            rm -r $(dirname "$kak_opt_spell_tmp_file")
        } </dev/null >/dev/null 2>&1 &
    }
}

def spell-replace %{%sh{
    suggestions=$(echo "$kak_selection" | aspell -a | grep '^&' | cut -d: -f2)
    menu=$(echo "${suggestions#?}" | awk -F', ' '
    {
        for (i=1; i<=NF; i++)
            printf "%s", "%{"$i"}" "%{exec -itersel c"$i"<esc>be}"
    }
    ')
    printf '%s\n' "try %{ menu -auto-single $menu }"
}}
