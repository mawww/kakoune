decl -hidden range-faces spell_regions
decl -hidden str spell_tmp_file

def -params ..1 spell -docstring "Check spelling of the current buffer with aspell (the first optional argument is the language against which the check will be performed)" %{
    try %{ addhl ranges 'spell_regions' }
    %sh{
        file=$(mktemp -d -t kak-spell.XXXXXXXX)/buffer
        printf 'eval -no-hooks write %s\n' "${file}"
        printf 'set buffer spell_tmp_file %s\n' "${file}"
    }
    %sh{
        if [ $# -ge 1 ]; then
            if [ ${#1} -ne 2 -a ${#1} -ne 5 ]; then
                echo 'echo -color Error Invalid language code (examples of expected format: en, en_US, en-US)'
                rm -r $(dirname $kak_opt_spell_tmp_file)
                exit 1
            else
                options="-l $1"
            fi
        fi
        sed 's/^/^/' < $kak_opt_spell_tmp_file | aspell -a $options 2>&1 | tee /tmp/spell-out | {
            line_num=1
            regions=$kak_timestamp
            read line # drop the identification message
            while read line; do
                case "$line" in
                    [\#\&]*)
                        if expr "$line" : '^&' >/dev/null; then
                           begin=$(printf %s\\n "$line" | cut -d ' ' -f 4 | sed 's/:$//')
                        else
                           begin=$(printf %s\\n "$line" | cut -d ' ' -f 3)
                        fi
                        word=$(printf %s\\n "$line" | cut -d ' ' -f 2)
                        end=$((begin + ${#word}))
                        regions="$regions:$line_num.$begin,$line_num.$end|Error"
                        ;;
                    '') line_num=$((line_num + 1));;
                    \*) ;;
                    *) printf 'echo -color Error %%{%s}\n' "${line}";;
                esac
            done
            printf 'set buffer spell_regions %%{%s}' "${regions}"
        }
        rm -r $(dirname $kak_opt_spell_tmp_file)
    }
}
