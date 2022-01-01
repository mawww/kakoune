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

        last_vim_et="" last_vim_sw="" last_vim_ts=""
        # Translate a vim option into the corresponding kakoune one
        translate_opt_vim() {
            key="$1"
            value="$2"
            tr=""

            case "${key}" in
                so|scrolloff)
                    key="scrolloff";
                    value="${value},${kak_opt_scrolloff##*,}";;
                siso|sidescrolloff)
                    key="scrolloff";
                    value="${kak_opt_scrolloff%%,*},${value}";;
                ts|tabstop)    key="tabstop"    ; last_vim_ts="${value}";;
                sw|shiftwidth) key="indentwidth"; last_vim_sw="${value}";;
                et|expandtab)     last_vim_et="1"; return;;
                noet|noexpandtab) last_vim_et="0"; return;;
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
            readonly key="$1"
            readonly value="$2"

            case "${key}" in
                scrolloff|tabstop|indentwidth|autowrap_column|eolformat|filetype|BOM|spell_lang);;
                *) printf 'echo -debug %s' "$(kakquote "Unsupported kakoune variable: ${key}")" \
                       | kak -p "${kak_session}"
                   return;;
            esac

            printf 'set-option buffer %s %s\n' "${key}" "$(kakquote "${value}")"
        }

        after_loop_ends() {
            # This does _not_ automatically convert tabs to spaces in insert mode, it only sets
            # `indentwidth` to indent with tabs or spaces (<lt> and <gt>).
            if [ "${type_selection}" = "vim" ]; then
                if [ "${last_vim_et}" = 0 ]; then
                    # vim `noexpandtab`: use tabs for indentation
                    printf 'set-option buffer %s %s\n' "indentwidth" "0"
                elif [ "${last_vim_et}" = 1 ]; then
                    # vim `expandtab`: use spaces for indentation.
                    # If `sw` is `0`, indent with spaces with the size of a tabstop.
                    if [ "${last_vim_sw}" = 0 ]; then
                        printf 'set-option buffer %s %s\n' "indentwidth" "%opt{tabstop}"
                    fi
                fi
            fi
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
        stop_looping=0
        printf %s "${kak_selection}" | sed          \
                -e 's/^[^:]\{1,\}://'               \
                -e 's/[ \t]*set\{0,1\}[ \t]//'      \
                -e 's/:[^a-zA-Z0-9_=-]*$//'         \
                -e 's/:/ /g'                        \
                | tr ' ' '\n'                       \
                | while read -r option || stop_looping=1; do
            if [ "${stop_looping}" = 1 ]; then
                after_loop_ends
                break
            fi
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
             s^\S*?\s+?\w+:\s?[^\n]+<ret> <a-x>
        evaluate-commands -draft -itersel modeline-parse-impl
    } }
}
