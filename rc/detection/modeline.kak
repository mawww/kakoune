##
## modeline.kak by lenormf
##

## Currently supported modeline format: vim
## Also supports kakoune options with a 'kak' or 'kakoune' prefix
## Only a few options are supported, in order to prevent the
## buffers from poking around the configuration too much

declare-option -docstring "amount of lines that will be checked at the beginning and the end of the buffer" \
    int modelines 5

define-command -hidden modeline-parse-impl %{
    evaluate-commands %sh{
        kakquote() { printf "%s" "$*" | sed "s/'/''/g; 1s/^/'/; \$s/\$/'/"; }

        # Translate a vim option into the corresponding kakoune one
        translate_opt_vim() {
            local key="$1"
            local value="$2"

            case "${key}" in
                so|scrolloff)
                    key="scrolloff";
                    value="${value},${kak_opt_scrolloff##*,}";;
                siso|sidescrolloff)
                    key="scrolloff";
                    value="${kak_opt_scrolloff%%,*},${value}";;
                ts|tabstop) key="tabstop";;
                sw|shiftwidth) key="indentwidth";;
                tw|textwidth) key="autowrap_column";;
                ff|fileformat)
                    key="eolformat";
                    case "${value}" in
                        unix) value="lf";;
                        dos) value="crlf";;
                        *)
                            printf '%s\n' "Unsupported file format: ${value}" >&2
                            return;;
                    esac
                ;;
                ft|filetype) key="filetype";;
                bomb)
                    key="BOM";
                    value="utf8";;
                nobomb)
                    key="BOM";
                    value="none";;
                spelllang|spl)
                    key="spell_lang";
                    value="${value%%,*}";;
                *)
                    printf '%s\n' "Unsupported vim variable: ${key}" >&2
                    return;;
            esac

            printf 'set-option buffer %s %s\n' "${key}" "$(kakquote "${value}")"
        }

        # Pass a few whitelisted options to kakoune directly
        translate_opt_kakoune() {
            local readonly key="$1"
            local readonly value="$2"

            case "${key}" in
                scrolloff|tabstop|indentwidth|autowrap_column|eolformat|filetype|BOM|spell_lang);;
                *) printf 'echo -debug %s' "$(kakquote "Unsupported kakoune variable: ${key}")" \
                       | kak -p "${kak_session}"
                   return;;
            esac

            printf 'set-option buffer %s %s\n' "${key}" "$(kakquote "${value}")"
        }

        case "${kak_selection}" in
            *vi:*|*vim:*) type_selection="vim";;
            *kak:*|*kakoune:*) type_selection="kakoune";;
            *)
                printf 'fail %s\n' "$(kakquote "Unsupported modeline format: ${kak_selection}")"
                exit 1 ;;
        esac

        # The following subshell will keep the actual options of the modeline, and strip:
        # - the text that leads the first option, according to the official vim modeline format
        # - the trailing text after the last option, and an optional ':' sign before it
        # It will also convert the ':' seperators beween the option=value pairs
        # More info: http://vimdoc.sourceforge.net/htmldoc/options.html#modeline
        printf %s "${kak_selection}" | sed                      \
                -e 's/^[^:]\{1,\}://'                           \
                -e 's/[ \t]*set\{0,1\}[ \t]\([^:]*\).*$/\1/'    \
                -e 's/:[^a-zA-Z0-9_=-]*$//'                     \
                -e 's/:/ /g'                                    \
                | tr ' ' '\n'                                   \
                | while read -r option; do
            name_option="${option%%=*}"
            value_option="${option#*=}"

            if [ -z "${option}" ]; then
                continue
            fi

            case "${type_selection}" in
                vim) translate_opt_vim "${name_option}" "${value_option}";;
                kakoune) translate_opt_kakoune "${name_option}" "${value_option}";;
                *) exit 1;;
            esac
        done
    }
}

# Add the following function to a hook on BufOpenFile to automatically parse modelines
# Select the first and last `modelines` lines in the buffer, only keep modelines
# ref. options.txt (in vim `:help options`) : 2 forms of modelines: 
#   [text]{white}{vi:|vim:|ex:}[white]{options}
#   [text]{white}{vi:|vim:|Vim:|ex:}[white]se[t] {options}:[text]
define-command modeline-parse -docstring "Read and interpret vi-format modelines at the beginning/end of the buffer" %{
    try %{ evaluate-commands -draft %{
        execute-keys <percent> "s(?S)\A(.+\n){,%opt{modelines}}|(.+\n){,%opt{modelines}}\z<ret>" \
             s^\S*?\s+?\w+:\s?[^\n]+<ret> x
        evaluate-commands -draft -itersel modeline-parse-impl
    } }
}
