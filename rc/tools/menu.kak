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
        cases=
        select_cases=
        completion=
        nl=$(printf '\n.'); nl=${nl%.}
        while [ $# -gt 0 ]; do
            title=$1
            command=$2
            completion="${completion}${title}${nl}"
            cases="${cases}
                $(kak -quote shell -- "$title"))
                    printf '%s\\n' $(kak -quote shell -- "$command")
                    ;;"
            if $select_cmds; then
                select_command=$3
                select_cases="${select_cases}
                    $(kak -quote shell -- "$title"))
                        printf '%s\\n' $(kak -quote shell -- "$select_command")
                        ;;"
            fi
            shift $stride
        done

        printf "%s" "prompt '' $(kak -quote kakoune -- "
            evaluate-commands %sh$(kak -quote kakoune -- '
                case "$kak_text" in
                '"${cases}"'
                *) echo fail -- no such item: "$(kak -quote kakoune -- "$kak_text")"
                esac
            ')
        ")"

        if $select_cmds; then
            printf "%s" " -on-change $(kak -quote kakoune -- "
                evaluate-commands %sh$(kak -quote kakoune -- '
                    case "$kak_text" in
                    '"$select_cases"'
                    *) : ;;
                    esac
                ')
           ")"
        fi

        if [ -n "$on_abort" ]; then
            printf "%s" " -on-abort $(kak -quote kakoune -- "$on_abort")"
        fi

        printf "%s\n" " -menu -shell-script-candidates $(kak -quote kakoune -- "
            printf %s $(kak -quote shell -- "$completion")
        ")"
    }
}
