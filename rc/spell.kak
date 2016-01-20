decl -hidden range-faces spell_regions
decl -hidden str spell_tmp_file

def spell %{
    try %{ addhl ranges 'spell_regions' }
    %sh{
        file=$(mktemp -d -t kak-spell.XXXXXXXX)/buffer
        echo "write ${file}"
        echo "set buffer spell_tmp_file ${file}"
    }
    %sh{
        sed -i 's/^/^/' $kak_opt_spell_tmp_file
        aspell -a < $kak_opt_spell_tmp_file 2>&1 | {
            line_num=1
            regions=$kak_timestamp
            while read line; do
                case $line in
                   \&*)
                       begin=$(printf %s "$line" | cut -d ' ' -f 4 | sed 's/:$//')
                       ;&
                   '#'*)
                       word=$(printf %s "$line" | cut -d ' ' -f 2)
                       begin=${begin:-$(printf %s "$line" | cut -d ' ' -f 3)}
                       end=$((begin + ${#word}))
                       # echo "echo -debug -- line: $line_num, word: $word, begin: $begin, end: $end"
                       regions="$regions:$line_num.$begin,$line_num.$end|Error"
                       ;;
                   '') ((++line_num)) ;;
                   *) ;;
                esac
            done
            echo "set buffer spell_regions %{$regions}"
        }
        rm -r $(dirname $kak_opt_spell_tmp_file)
    }
}
