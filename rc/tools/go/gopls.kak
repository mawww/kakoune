# gopls.kak: gopls bindings for kakoune

define-command -params 1 -docstring %{
gopls <command>: gopls command wrapper

All commands are forwarded to gopls utility
Available commands are:
    format
    imports
    definition
    references
} -shell-script-candidates %{
    printf "format\nimports\ndefinition\nreferences\n"
} \
gopls %{
    require-module gopls
    evaluate-commands %sh{
        case "$1" in
        format|imports)
            printf %s\\n "gopls-cmd $1"
            ;;
        definition)
            printf %s\\n "gopls-def"
            ;;
        references)
            printf %s\\n "gopls-ref"
            ;;
        *)
            printf "fail Unknown gopls command '%s'\n" "$1"
            exit
            ;;
        esac
    }
}

provide-module gopls %§

evaluate-commands %sh{
    if ! command -v gopls > /dev/null 2>&1; then
        echo "fail Please install gopls or add to PATH!"
    fi
}

# Temp dir preparation
declare-option -hidden str gopls_tmp_dir
define-command -hidden -params 0 gopls-prepare %{
    evaluate-commands %sh{
        dir=$(mktemp -d "${TMPDIR:-/tmp}"/kak-gopls.XXXXXXXX)
        printf %s\\n "set-option buffer gopls_tmp_dir ${dir}"
    }
}

# gopls format/imports
define-command -hidden -params 1 gopls-cmd %{
    gopls-prepare
    evaluate-commands %sh{
        dir=${kak_opt_gopls_tmp_dir}
        gopls "$1" -w "${kak_buffile}" 2> "${dir}/stderr"
        if [ $? -ne 0 ]; then
            # show error messages in *debug* buffer
            printf %s\\n "echo -debug %file{${dir}/stderr}"
        fi
    }
    edit!
    nop %sh{ rm -rf "${kak_opt_gopls_tmp_dir}" }
}

# gopls definition
define-command -hidden -params 0 gopls-def %{
    evaluate-commands %sh{
        jump=$( gopls definition "${kak_buffile}:${kak_cursor_line}:${kak_cursor_column}" 2> /dev/null \
            |sed -e 's/-.*//; s/:/ /g; q' )
        if [ -n "${jump}" ]; then
            printf %s\\n "evaluate-commands -try-client '${kak_opt_jumpclient}' %{
                edit ${jump}
            }"
        fi
    }
}

# gopls references
define-command -hidden -params 0 gopls-ref %{
    gopls-prepare
    evaluate-commands %sh{
        dir=${kak_opt_gopls_tmp_dir}
        mkfifo "${dir}/fifo"
        ( gopls references "${kak_buffile}:${kak_cursor_line}:${kak_cursor_column}" \
            > "${dir}/fifo" 2> /dev/null & ) > /dev/null 2>&1 < /dev/null
        # using filetype=grep for nice hilight and <ret> mapping
        printf %s\\n "evaluate-commands -try-client '${kak_opt_toolsclient}' %{
            edit! -fifo '${dir}/fifo' *gopls-refs*
            set-option buffer filetype grep
            hook -always -once buffer BufCloseFifo .* %{ nop %sh{ rm -r '${dir}' } }
        }"
    }
}

§
