declare-option -hidden range-specs spell_regions
declare-option -hidden str spell_last_lang

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
        use_lang() {
            if ! printf %s "$1" | grep -qE '^[a-z]{2,3}([_-][A-Z]{2})?$'; then
                echo "fail 'Invalid language code (examples of expected format: en, en_US, en-US)'"
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

        printf 'eval -no-hooks write %s\n' "${kak_response_fifo}" > $kak_command_fifo

        {
            sed 's/^/^/' | eval "aspell --byte-offsets -a $options" 2>&1 | awk '
                BEGIN {
                    line_num = 1
                    regions = ENVIRON["kak_timestamp"]
                    server_command = sprintf("kak -p \"%s\"", ENVIRON["kak_session"])
                }

                {
                    if (/^@\(#\)/) {
                        # drop the identification message
                    }

                    else if (/^\*/) {
                        # nothing
                    }

                    else if (/^[+-]/) {
                        # required to ignore undocumented aspell functionality
                    }

                    else if (/^$/) {
                        line_num++
                    }

                    else if (/^[#&]/) {
                        word_len = length($2)
                        word_pos = substr($0, 1, 1) == "&" ? substr($4, 1, length($4) - 1) : $3;
                        regions = regions " " line_num "." word_pos "+" word_len "|DiagnosticError"
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
        } <$kak_response_fifo >/dev/null 2>&1 &
    }
}

define-command spell-clear %{
    unset-option buffer spell_regions
}

define-command spell-next %{ evaluate-commands %sh{
    anchor_line="${kak_selection_desc%%.*}"
    anchor_col="${kak_selection_desc%%,*}"
    anchor_col="${anchor_col##*.}"

    start_first="${kak_opt_spell_regions%%|*}"
    start_first="${start_first#* }"

    # Make sure properly formatted selection descriptions are in `%opt{spell_regions}`
    if ! printf %s "${start_first}" | grep -qE '^[0-9]+\.[0-9]+,[0-9]+\.[0-9]+$'; then
        exit
    fi

    printf %s "${kak_opt_spell_regions#* }" | awk -v start_first="${start_first}" \
                                                  -v anchor_line="${anchor_line}" \
                                                  -v anchor_col="${anchor_col}" '
        BEGIN {
            anchor_line = int(anchor_line)
            anchor_col = int(anchor_col)
        }

        {
            for (i = 1; i <= NF; i++) {
                sel = $i
                sub(/\|.+$/, "", sel)

                start_line = sel
                sub(/\..+$/, "", start_line)
                start_line = int(start_line)

                start_col = sel
                sub(/,.+$/, "", start_col)
                sub(/^.+\./, "", start_col)
                start_col = int(start_col)

                if (start_line < anchor_line \
                    || (start_line == anchor_line && start_col <= anchor_col))
                    continue

                target_sel = sel
                break
            }
        }

        END {
            if (!target_sel)
                target_sel = start_first

            printf "select %s\n", target_sel
        }'
} }

define-command \
    -docstring "Suggest replacement words for the current selection, against the last language used by the spell-check command" \
    spell-replace %{
    prompt \
        -shell-script-candidates %{
            options=""
            if [ -n "$kak_opt_spell_last_lang" ]; then
                options="-l '$kak_opt_spell_last_lang'"
            fi
            printf %s "$kak_selection" |
                eval "aspell -a $options" |
                sed -n -e '/^&/ { s/^[^:]*: //; s/, /\n/g; p }'
        } \
        "Replace with: " \
        %{
            evaluate-commands -save-regs a %{
                set-register a %val{text}
                execute-keys c <c-r>a <esc>
            }
        }
}


define-command -params 0.. \
    -docstring "Add the current selection to the dictionary" \
    spell-add %{ evaluate-commands %sh{
    options=""
    if [ -n "$kak_opt_spell_last_lang" ]; then
        options="-l '$kak_opt_spell_last_lang'"
    fi
    if [ $# -eq 0 ]; then
        # use selections
        eval set -- "$kak_quoted_selections"
    fi
    while [ $# -gt 0 ]; do
        word="$1"
        if ! printf '*%s\n#\n' "${word}" | eval "aspell -a $options" >/dev/null; then
           printf 'fail "Unable to add word: %s"' "$(printf %s "${word}" | sed 's/"/&&/g')"
           exit 1
        fi
        shift
    done
}}
