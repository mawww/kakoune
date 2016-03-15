##
## modeline.kak by lenormf
##

## Currently supported modeline format: vim
## Also supports kakoune options with a 'kak' or 'kakoune' prefix
## Only a few options are supported, in order to prevent the
## buffers from poking around the configuration too much

# Amount of additional lines that will be checked at the beginning
# and the end of the buffer
decl int modelines 5

def -hidden _modeline-parse %{
    %sh{
        # Translate a vim option into the corresponding kakoune one
        function translate_opt_vim {
            readonly key="$1"
            readonly value="$2"
            local tr=""

            case "${key}" in
                so) ;&
                scrolloff) tr="scrolloff ${value},${kak_opt_scrolloff##*,}";;
                siso) ;&
                sidescrolloff) tr="scrolloff ${kak_opt_scrolloff%%,*},${value}";;
                ts) ;&
                tabstop) tr="tabstop ${value}";;
                sw) ;&
                shiftwidth) tr="indentwidth ${value}";;
                tw) ;&
                textwidth) tr="autowrap_column ${value}";;
                ff) ;&
                fileformat)
                    case "${value}" in
                        unix) tr="eolformat lf";;
                        dos) tr="eolformat crlf";;
                        *) printf %s "Unsupported file format: ${value}" >&2;;
                    esac
                ;;
                ft) ;&
                filetype) tr="filetype ${value}";;
                bomb) tr="BOM utf8";;
                nobomb) tr="BOM none";;
                *) printf %s "Unsupported vim variable: ${key}" >&2;;
            esac

            [ -n "${tr}" ] && printf %s "set buffer ${tr}"
        }

        # Pass a few whitelisted options to kakoune directly
        function translate_opt_kakoune {
            readonly key="$1"
            readonly value="$2"
            readonly OPTS_ALLOWED=(
                scrolloff
                tabstop
                indentwidth
                autowrap_column
                eolformat
                filetype
                mimetype
                BOM
            )

            grep -qw "${key}" <<< "${OPTS_ALLOWED[@]}" || {
                printf %s "Unsupported kakoune variable: ${key}" >&2;
                return;
            }

            printf %s "set buffer ${key} ${value}"
        }

        # The following subshell will keep the actual options of the modeline, and strip:
        # - the text that leads the first option, according to the official vim modeline format
        # - the trailing text after the last option, and an optional ':' sign before it
        # It will also convert the ':' seperators beween the option=value pairs
        # More info: http://vimdoc.sourceforge.net/htmldoc/options.html#modeline
        options=(
            $(sed -r -e 's/^(.+\s\w+:\s?(set?)?\s)//' \
                     -e 's/:?\s[^a-zA-Z0-9_=-]+$//' \
                     -e 's/:/ /g' <<< "${kak_selection}")
        )

        case "${kak_selection}" in
            *vi:*) ;&
            *vim:*) type_selection="vim";;
            *kak:*) ;&
            *kakoune:*) type_selection="kakoune";;
            *) printf %s "echo -debug Unsupported modeline format";;
        esac
        [ -n "${type_selection}" ] || exit 1

        for option in "${options[@]}"; do
            name_option="${option%%=*}"
            value_option="${option#*=}"

            case "${type_selection}" in
                vim) tr=$(translate_opt_vim "${name_option}" "${value_option}");;
                kakoune) tr=$(translate_opt_kakoune "${name_option}" "${value_option}");;
            esac

            [ -n "${tr}" ] && printf %s "${tr}"
        done
    }
}

# Add the following function to a hook on BufOpen to automatically parse modelines
# Select the first and last `modelines` lines in the buffer, only keep modelines
def modeline-parse %{
    try %{ eval -draft %{
        exec \%s\`|.\'<ret> %opt{modelines}k <a-x> %opt{modelines}X \
             s^[^\s]+?\s(vim?|kak(oune)?):\s?[^\n]+<ret>
        eval -draft -itersel _modeline-parse
    } }
}
