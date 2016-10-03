decl -hidden range-faces spell_regions
decl -hidden str spell_tmp_file

def -params ..1 spell -docstring "Check spelling of the current buffer with aspell (the first optional argument is the language against which the check will be performed)" %{
    try %{ addhl ranges 'spell_regions' }
    %sh{
        file=$(mktemp -d -t kak-spell.XXXXXXXX)/buffer
        printf %s\\n "eval -no-hooks write ${file}"
        printf %s\\n "set buffer spell_tmp_file ${file}"
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
        sed -i 's/^/^/' $kak_opt_spell_tmp_file
        aspell -a $options < $kak_opt_spell_tmp_file 2>&1 | {
            line_num=1
            regions=$kak_timestamp
            while read line; do
                case $line in
                   \&*)
                       begin=$(printf %s\\n "$line" | cut -d ' ' -f 4 | sed 's/:$//')
                       ;&
                   '#'*)
                       word=$(printf %s\\n "$line" | cut -d ' ' -f 2)
                       begin=${begin:-$(printf %s\\n "$line" | cut -d ' ' -f 3)}
                       end=$((begin + ${#word}))
                       # printf %s\\n "echo -debug -- line: $line_num, word: $word, begin: $begin, end: $end"
                       regions="$regions:$line_num.$begin,$line_num.$end|Error"
                       ;;
                   '') ((++line_num)) ;;
                   *) ;;
                esac
            done
            printf %s\\n "set buffer spell_regions %{$regions}"
        }
        rm -r $(dirname $kak_opt_spell_tmp_file)
    }
}
