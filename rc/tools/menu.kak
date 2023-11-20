provide-module menu %§§

define-command menu -params 1.. -docstring %{
    menu [<switches>] <name1> <commands1> <name2> <commands2>...: display a 
    menu and execute commands for the selected item

    -auto-single instantly validate if only one item is available
    -select-cmds each item specify an additional command to run when selected
} %{
    evaluate-commands %sh{
        auto_single=false
        select_cmds=false
        stride=2
        on_abort=
        while true
        do
            case "$1" in
                (-auto-single) auto_single=true ;;
                (-select-cmds) select_cmds=true; stride=3 ;;
                (-on-abort) on_abort="$2"; shift ;;
                (-markup) ;; # no longer supported
                (*) break ;;
            esac
            shift
        done
        if [ $(( $# % $stride )) -ne 0 ]; then
            echo fail "wrong argument count"
            exit
        fi
        if $auto_single && [ $# -eq $stride ]; then
            printf %s "$2"
            exit
        fi
        shellquote() {
            printf "'%s'" "$(printf %s "$1" | sed "s/'/'\\\\''/g; s/§/§§/g; $2")"
        }
        cases=
        select_cases=
        completion=
        nl=$(printf '\n.'); nl=${nl%.}
        while [ $# -gt 0 ]; do
            title=$1
            command=$2
            completion="${completion}${title}${nl}"
            cases="${cases}
                ($(shellquote "$title" s/¶/¶¶/g))
                    printf '%s\\n' $(shellquote "$command" s/¶/¶¶/g)
                    ;;"
            if $select_cmds; then
                select_command=$3
                select_cases="${select_cases}
                    ($(shellquote "$title" s/¶/¶¶/g))
                        printf '%s\\n' $(shellquote "$select_command" s/¶/¶¶/g)
                        ;;"
            fi
            shift $stride
        done
        printf "\
            prompt '' %%§
                    evaluate-commands %%sh¶
                        case \"\$kak_text\" in \
                        %s
                        (*) echo fail -- no such item: \"'\$(printf %%s \"\$kak_text\" | sed \"s/'/''/g\")'\" ;;
                        esac
                    ¶
                §" "$cases"
        if $select_cmds; then
            printf " \
                    -on-change %%§
                        evaluate-commands %%sh¶
                            case \"\$kak_text\" in \
                            %s
                            (*) : ;;
                            esac
                        ¶
                    §" "$select_cases"
        fi
        if [ -n "$on_abort" ]; then
            printf " -on-abort '%s'" "$(printf %s "$on_abort" | sed "s/'/''/g")"
        fi
        printf ' -menu -shell-script-candidates %%§
                    printf %%s %s
                §\n' "$(shellquote "$completion")"
    }
}
