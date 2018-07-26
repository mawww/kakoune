declare-option -hidden range-specs spell_regions
declare-option -hidden str spell_lang
declare-option -hidden str spell_tmp_file

define-command -params ..1 \
    -docstring %{spell [<language>]: spell check the current buffer
The first optional argument is the language against which the check will be performed
Formats of language supported:
 - ISO language code, e.g. 'en'
 - language code above followed by a dash or underscore with an ISO country code, e.g. 'en-US'} \
    spell %{
    try %{ add-highlighter window/ ranges 'spell_regions' }
    evaluate-commands %sh{
        file=$(mktemp -d "${TMPDIR:-/tmp}"/kak-spell.XXXXXXXX)/buffer
        printf 'eval -no-hooks write -sync %s\n' "${file}"
        printf 'set-option buffer spell_tmp_file %s\n' "${file}"
    }
    evaluate-commands %sh{
        if [ $# -ge 1 ]; then
            if [ ${#1} -ne 2 ] && [ ${#1} -ne 5 ]; then
                echo "echo -markup '{Error}Invalid language code (examples of expected format: en, en_US, en-US)'"
                rm -rf "$(dirname "$kak_opt_spell_tmp_file")"
                exit 1
            else
                options="-l '$1'"
                printf 'set-option buffer spell_lang %s\n' "$1"
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
                            regions="$regions $line_num.$pos+${len}|Error"
                            ;;
                        '') line_num=$((line_num + 1));;
                        \*) ;;
                        *) printf 'echo -markup %%{{Error}%s}\n' "${line}" | kak -p "${kak_session}";;
                    esac
                done
                printf 'set-option "buffer=%s" spell_regions %s' "${kak_bufname}" "${regions}" \
                    | kak -p "${kak_session}"
            }
            rm -rf $(dirname "$kak_opt_spell_tmp_file")
        } </dev/null >/dev/null 2>&1 &
    }
}

define-command spell-next %{ evaluate-commands %sh{
    anchor_line="${kak_selection_desc%%.*}"
    anchor_col="${kak_selection_desc%%,*}"
    anchor_col="${anchor_col##*.}"

    start_first="${kak_opt_spell_regions#* }"
    start_first="${start_first%%|*}"
    start_first="${start_first#\'}"

    find_next_word_desc() {
        ## XXX: the `spell` command adds sorted selection descriptions to the range
        printf %s\\n "${1}" \
            | sed -e "s/'//g" -e 's/^[0-9]* //' -e 's/|[^ ]*//g' \
            | tr ' ' '\n' \
            | while IFS=, read -r start end; do
                start_line="${start%.*}"
                start_col="${start#*.}"
                end_line="${end%.*}"
                end_col="${end#*.}"

                if [ "${start_line}" -lt "${anchor_line}" ]; then
                    continue
                elif [ "${start_line}" -eq "${anchor_line}" ] \
                    && [ "${start_col}" -le "${anchor_col}" ]; then
                    continue
                fi

                printf 'select %s,%s\n' "${start}" "${end}"
                break
            done
    }

    # no selection descriptions are in `spell_regions`
    if ! expr "${start_first}" : '[0-9][0-9]*\.[0-9][0-9]*,[0-9][0-9]*\.[0-9]' >/dev/null; then
        exit
    fi

    next_word_desc=$(find_next_word_desc "${kak_opt_spell_regions}")
    if [ -n "${next_word_desc}" ]; then
        printf %s\\n "${next_word_desc}"
    else
        printf 'select %s\n' "${start_first}"
    fi
} }

define-command spell-replace %{ evaluate-commands %sh{
    if [ -n "$kak_opt_spell_lang" ]; then
        options="-l '$kak_opt_spell_lang'"
    fi
    suggestions=$(printf %s "$kak_selection" | eval "aspell -a $options" | grep '^&' | cut -d: -f2)
    menu=$(printf %s "${suggestions#?}" | awk -F', ' '
        {
            for (i=1; i<=NF; i++)
                printf "%s", "%{"$i"}" "%{execute-keys -itersel %{c"$i"<esc>be}}"
        }
    ')
    printf 'try %%{ menu -auto-single %s }' "${menu}"
} }
