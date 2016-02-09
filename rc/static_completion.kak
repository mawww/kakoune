## FIXME: this variable should be a `flags` type var
# Enumeration that dictates what keywords are statically completed upon
decl str static_complete ''

## The following function dynamically declares functions that handle static completion in buffers
## $1: filetype of the buffer (e.g. python, sh), must be used by a loaded .kak script
## $2..$#: key=value pairs, with `key` a type of token (will be used to generate the flags in `static_complete`
## and `value` a kakoune list of tokens to be completed upon (abc:def:foo)
def -hidden -params 2.. static-complete-initialize %{
    %sh{
        filetype="$1"
        shift

        # pipe separated list of flags, generated after the keys passed to the functions
        flagslist=""
        # arrays of kakoune style list, declared with the shell syntax to be embedded into the function generated below
        inline_values=""
        for i in "$@"; do
            key="${i%%=*}"
            value="${i#*=}"

            [ -z "${flagslist}" ] && flagslist="${key}" || flagslist="${flagslist}|${key}"
            [ -z "${inline_values}" ] && inline_values="${key}='${value}'" || inline_values="${inline_values}; ${key}='${value}'"
        done

        # Add the corresponding keywords to the `static_words` list every time a buffer is opened
        echo "
            def -hidden _${filetype}_set_static_completion %{ %sh{
                echo \"set window static_words ''\"
                ${inline_values};
                for flag in \${kak_opt_static_complete//|/ }; do
                    keywords_hl=\${!flag}
                    if [ -n \"\${keywords_hl}\" ]; then
                        echo \"set -add window static_words '\${keywords_hl}'\"
                    else
                        echo \"Unsupported flag: \${flag}\" >&2
                        break
                    fi
                done
            } }
            # If no flags have been set in the user configuration, activate all the flags
            hook global WinSetOption filetype=${filetype} %{ %sh{
                if [ -z \"\${kak_opt_static_complete}\" ]; then
                    echo \"set window static_complete '${flagslist}'\"
                fi
            } }
        "
    }
}

# Refresh the completed words everytime the `static_complete` option is modified
hook global WinSetOption static_complete=.+ %{ %sh{
    if [ -n "${kak_opt_filetype}" ]; then
        echo "try %{ _${kak_opt_filetype}_set_static_completion }"
    fi
} }
