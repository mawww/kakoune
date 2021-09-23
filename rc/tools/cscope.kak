#  Kakoune cscope support script
#
# This script requires the readtags command available in universal-ctags

declare-option -docstring "list of paths to cscope files to parse when looking up a symbol" \
    str-list cscopefiles './cscope.out'

declare-option -docstring "shell command to run" str readcscopecmd "cscope -d -L"

define-command -params 1..2 \
    -shell-script-candidates %{
        if [ $kak_token_to_complete -eq 1 ]; then
            realpath() { ( cd "$(dirname "$1")"; printf "%s/%s\n" "$(pwd -P)" "$(basename "$1")" ) }
            eval "set -- ${kak_quoted_opt_ctagsfiles}"
            for candidate in "$@"; do
                [ -f "$candidate" ] && realpath "$candidate"
            done | awk '!x[$0]++' | # remove duplicates
            while read -r tags; do
                namecache="${tags%/*}/.kak.${tags##*/}.namecache"
                if [ -z "$(find "$namecache" -prune -newer "$tags")" ]; then
                    cut -f 1 "$tags" | grep -v '^!' | uniq > "$namecache"
                fi
                cat "$namecache"
            done
        fi} \
    -docstring %{
        cscope-search <query> [<symbol>]: jump to symbol's use
        query: a digit as below
            0 - Find this C symbol
            1 - Find this function definition
            2 - Find functions called by this function
            3 - Find functions calling this function
            4 - Find this text string
            6 - Find this egrep pattern
            7 - Find this file
            8 - Find files
            9 - Find assignments to this symbol
        symbol: If no symbol is passed then the current selection is used as symbol name
    } \
    cscope-search %[ evaluate-commands %sh[
        realpath() { ( cd "$(dirname "$1")"; printf "%s/%s\n" "$(pwd -P)" "$(basename "$1")" ) }
        export query="$1"
        export tagname="${2:-${kak_selection}}"
        eval "set -- ${kak_quoted_opt_cscopefiles}"
        for candidate in "$@"; do
            [ -f "$candidate" ] && realpath "$candidate"
        done | awk '!x[$0]++' | # remove duplicates
        while read -r tags; do
            printf '!TAGROOT\t%s\n' "$(realpath "${tags%/*}")/"
            [[ $query =~ ^[0-9]$ ]] && ${kak_opt_readcscopecmd} -f "$tags" -$query "$tagname"
        done | awk -F ' |\t|\n' '
            /^!TAGROOT\t/ { tagroot=$2 }
            /^[^ ]+ [^ ]+ [0-9]+ .*/ {
                line = $0; sub("[^ ]+ [^ ]+ [0-9]+ ", "", line)
                menu_info = line; gsub("!", "!!", menu_info); gsub(/^[\t ]+/, "", menu_info); gsub(/\t/, " ", menu_info);
                keys = line; gsub(/</, "<lt>", keys); gsub(/\t/, "<c-v><c-i>", keys); gsub("!", "!!", keys); gsub("&", "&&", keys); gsub("#", "##", keys); gsub("\\|", "||", keys); gsub("\\\\/", "/", keys);
                menu_item = $1; gsub("!", "!!", menu_item);
                edit_path = path($1); gsub("&", "&&", edit_path); gsub("#", "##", edit_path); gsub("\\|", "||", edit_path);
                select = $4; for(i = 5; i <= NF; ++i) {select = select " " $i }; gsub(/</, "<lt>", select); gsub(/\t/, "<c-v><c-i>", select); gsub("!", "!!", select); gsub("&", "&&", select); gsub("#", "##", select); gsub("\\|", "||", select);
                out = out "%!" menu_item ": {MenuInfo}{\\}" menu_info "! %!evaluate-commands %# try %& edit -existing %|" edit_path "|; execute-keys %|/\\Q" keys "<ret>vc| & catch %& fail unable to find tag &; try %& execute-keys %|s\\Q" select "<ret>| & # !"
            }
            END { print ( length(out) == 0 ? "fail no such symbol " ENVIRON["tagname"] : "menu -markup -auto-single " out ) }
            # Ensure x is an absolute file path, by prepending with tagroot
            function path(x) { return x ~/^\// ? x : tagroot x }'
    ]]

declare-option -docstring "shell command to run" \
    str cscopecmd "cscope -R -q -b"

define-command -params ..1 \
    -shell-script-candidates %{
        realpath() { ( cd "$(dirname "$1")"; printf "%s/%s\n" "$(pwd -P)" "$(basename "$1")" ) }
        eval "set -- ${kak_quoted_opt_cscopefiles}"
        for candidate in "$@"; do
            # if the candidate directory exists, pass it along
            cscoperoot=$(dirname "$candidate")
            [ -d "$cscoperoot" ] && realpath "$candidate"
        done | awk '!x[$0]++' | # remove duplicates
        while read -r cscope; do
            echo "$cscope"
        done} \
    -docstring %{
        cscope-generate [<path>]: Generate cscope file asynchronously
        If no path is passed then all cscope files are regenerated
    } \
    cscope-generate %{ echo -markup "{Information}launching cscope generation in the background, this may take awhile"
        nop %sh{ (
        realpath() { ( cd "$(dirname "$1")"; printf "%s/%s\n" "$(pwd -P)" "$(basename "$1")" ) }
        cscopefile="$1"
        cscoperoot=$(dirname "$cscopefile")
        if [ ! -z $cscopefile ] && [ -d $cscoperoot ] && [[ ! ${kak_quoted_opt_cscopefiles} =~ "'$cscopefile'" ]]; then
            # if this is a new file for an existing directory, add it to the candidates
            printf %s\\n "evaluate-commands -client ${kak_client} set-option -add current cscopefiles $cscopefile" | kak -p ${kak_session}
            kak_quoted_opt_cscopefiles="${kak_quoted_opt_cscopefiles} '$cscopefile'"
            cscopefile=$(realpath "$cscopefile")
        fi
        eval "set -- ${kak_quoted_opt_cscopefiles}"
        msg=$(for candidate in "$@"; do
            # if no file is specified or it matches the candidate, and the candidate directory exists, pass it along
            croot=$(dirname "$candidate")
            [ -z "$cscopefile" -o "$cscopefile" = "$candidate" ] && [ -d "$croot" ] && realpath "$candidate"
        done | awk '!x[$0]++' | # remove duplicates
        while read -r cscope; do
            while ! mkdir ${cscope}.kaklock 2>/dev/null; do sleep 1; done
            trap 'rmdir ${cscope}.kaklock' EXIT

            # generate the candidate
            pushd $(dirname $cscope)
            if ${kak_opt_cscopecmd} -f ${cscope}.kaktmp; then
                mv ${cscope}.kaktmp $cscope
                mv ${cscope}.kaktmp.in $cscope.in
                mv ${cscope}.kaktmp.po $cscope.po
                printf "S"
            else
                printf "X"
            fi
            popd

            rmdir ${cscope}.kaklock
        done)
        if [ -z "$msg" ] || [[ "$msg" == *"X"* ]]; then
            msg="cscope generation failed"
        else
            msg="cscope generation complete"
        fi
        printf %s\\n "evaluate-commands -client ${kak_client} echo -markup '{Information}${msg}'" | kak -p ${kak_session}
        ) > /dev/null 2>&1 < /dev/null & }
    }

