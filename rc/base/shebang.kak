##
## shebang.kak by lenormf
## Detect a shebang in a buffer and assign the proper filetype accordingly
##

def shebang-detect -docstring "Detect the shebang at the top of the buffer and assign the proper filetype accordingly" %{ %sh{
    if [ -n "${kak_opt_filetype}" ]; then
        exit
    fi

    printf %s "eval -draft -save-regs m %{ try %{
        exec -save-regs '/' gg <a-x> s^#!\s*[^\n-]+<ret> \;be\"my
        %sh{
            filetype=''
            case \"\${kak_reg_m}\" in
                zsh|bash|csh|ksh) filetype=sh;;
                python2|python3) filetype=python;;
                *) filetype=\"\${kak_reg_m}\";;
            esac
            if [ -n \"\${filetype}\" ]; then
                printf %s \"set buffer filetype \${filetype}\"
            fi
        }
    } }"
} }

hook global BufCreate .+ shebang-detect
hook global BufWritePost .+ shebang-detect
