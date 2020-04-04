declare-option -hidden range-specs spell_regions
declare-option -hidden str spell_last_lang
declare-option -hidden str spell_tmp_file

declare-option -docstring "default language to use when none is passed to the spell-check command" str spell_lang

define-command -params ..1 -docstring %{
    spell [<language>]: spell check the current buffer

    The first optional argument is the language against which the check will be performed (overrides `spell_lang`)
    Formats of language supported:
      - ISO language code, e.g. 'en'
      - language code above followed by a dash or underscore with an ISO country code, e.g. 'en-US'
    } spell %{
    try %{ add-highlighter window/ ranges 'spell_regions' }
    evaluate-commands %sh{
        file=$(mktemp -d "${TMPDIR:-/tmp}"/kak-spell.XXXXXXXX)/buffer
        printf 'eval -no-hooks write -sync %s\n' "${file}"
        printf 'set-option buffer spell_tmp_file %s\n' "${file}"
    }
    evaluate-commands %sh{
        use_lang() {
            if ! printf %s "$1" | grep -qE '^[a-z]{2,3}([_-][A-Z]{2})?$'; then
                echo "fail 'Invalid language code (examples of expected format: en, en_US, en-US)'"
                rm -rf "$(dirname "$kak_opt_spell_tmp_file")"
                exit 1
            else
                options="-l '$1'"
                printf 'set-option buffer spell_last_lang %s\n' "$1"
            fi
        }

        if [ $# -ge 1 ]; then
            use_lang "$1"
        elif [ -n "${kak_opt_spell_lang}" ]; then
            use_lang "${kak_opt_spell_lang}"
        fi

        {
            sed 's/^/^/' "$kak_opt_spell_tmp_file" | eval "aspell --byte-offsets -a $options" 2>&1 | awk '
                BEGIN {
                    line_num = 1
                    regions = ENVIRON["kak_timestamp"]
                    server_command = sprintf("kak -p \"%s\"", ENVIRON["kak_session"])
                }

                {
                    if (/^@\(#\)/) {
                        /* drop the identification message */
                    }

                    else if (/^\*/) {
                        /* nothing */
                    }

                    else if (/^$/) {
                        line_num++
                    }

                    else if (/^[#&]/) {
                        word_len = length($2)
                        word_pos = substr($0, 1, 1) == "&" ? substr($4, 1, length($4) - 1) : $3;
                        regions = regions " " line_num "." word_pos "+" word_len "|Error"
                    }

                    else {
                        line = $0
                        gsub(/"/, "&&", line)
                        command = "fail \"" line "\""
                        exit
                    }
                }

                END {
                    if (!length(command))
                        command = "set-option \"buffer=" ENVIRON["kak_bufname"] "\" spell_regions " regions

                    print command | server_command
                    close(server_command)
                }
            '
            rm -rf $(dirname "$kak_opt_spell_tmp_file")
        } </dev/null >/dev/null 2>&1 &
    }
}

define-command spell-clear %{
    unset-option buffer spell_regions
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

define-command \
    -docstring "Suggest replacement words for the current selection, against the last language used by the spell-check command" \
    spell-replace %{ evaluate-commands %sh{
    if [ -n "$kak_opt_spell_last_lang" ]; then
        options="-l '$kak_opt_spell_last_lang'"
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
