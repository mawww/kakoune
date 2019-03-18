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
        # Translate a vim option into the corresponding kakoune one
        translate_opt_vim() {
            readonly key="$1"
            readonly value="$2"
            tr=""

            case "${key}" in
                so|scrolloff) tr="scrolloff ${value},${kak_opt_scrolloff##*,}";;
                siso|sidescrolloff) tr="scrolloff ${kak_opt_scrolloff%%,*},${value}";;
                ts|tabstop) tr="tabstop ${value}";;
                sw|shiftwidth) tr="indentwidth ${value}";;
                tw|textwidth) tr="autowrap_column ${value}";;
                ff|fileformat)
                    case "${value}" in
                        unix) tr="eolformat lf";;
                        dos) tr="eolformat crlf";;
                        *) printf %s\\n "echo -debug 'Unsupported file format: ${value}'";;
                    esac
                ;;
                ft|filetype) tr="filetype ${value}";;
                bomb) tr="BOM utf8";;
                nobomb) tr="BOM none";;
                *) printf %s\\n "echo -debug 'Unsupported vim variable: ${key}'";;
            esac

            [ -n "${tr}" ] && printf %s\\n "set-option buffer ${tr}"
        }

        # Pass a few whitelisted options to kakoune directly
        translate_opt_kakoune() {
            readonly key="$1"
            readonly value="$2"

            case "${key}" in
                scrolloff|tabstop|indentwidth|autowrap_column|eolformat|filetype|BOM);;
                *) printf %s\\n "echo -debug 'Unsupported kakoune variable: ${key}'"
                   return;;
            esac

            printf %s\\n "set-option buffer ${key} ${value}"
        }

        case "${kak_selection}" in
            *vi:*|*vim:*) type_selection="vim";;
            *kak:*|*kakoune:*) type_selection="kakoune";;
            *) echo "echo -debug Unsupported modeline format";;
        esac
        [ -n "${type_selection}" ] || exit 1

        # The following subshell will keep the actual options of the modeline, and strip:
        # - the text that leads the first option, according to the official vim modeline format
        # - the trailing text after the last option, and an optional ':' sign before it
        # It will also convert the ':' seperators beween the option=value pairs
        # More info: http://vimdoc.sourceforge.net/htmldoc/options.html#modeline
        printf %s "${kak_selection}" | sed          \
                -e 's/^[^:]\{1,\}://'               \
                -e 's/[ \t]*set\{0,1\}[ \t]//'      \
                -e 's/:[^a-zA-Z0-9_=-]*$//'         \
                -e 's/:/ /g'                        \
                | tr ' ' '\n'                       \
                | while read -r option; do
            name_option="${option%%=*}"
            value_option="${option#*=}"

            [ -z "${option}" ] && continue

            case "${type_selection}" in
                vim) tr=$(translate_opt_vim "${name_option}" "${value_option}");;
                kakoune) tr=$(translate_opt_kakoune "${name_option}" "${value_option}");;
            esac

            [ -n "${tr}" ] && printf %s\\n "${tr}"
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
        execute-keys \%s\A|.\z<ret> %opt{modelines}k <a-x> %opt{modelines}X \
             s^\S*?\s+?(vim?|kak(oune)?):\s?[^\n]+<ret> <a-x>
        evaluate-commands -draft -itersel modeline-parse-impl
    } }
}
