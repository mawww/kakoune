# Kakoune Exuberant CTags support script
#
# This script requires the readtags command available in ctags source but
# not installed by default

decl str-list ctagsfiles 'tags'

def -shell-params \
    -shell-completion 'readtags -p "$1" | cut -f 1 | sort | uniq' \
    -docstring 'jump to tag definition' \
    tag \
    %{ %sh{
        export tagname=${1:-${kak_selection}}
        (
            IFS=':'
            for tags in ${kak_opt_ctagsfiles}; do
                readtags -t "${tags}" ${tagname}
            done
        ) | awk -F '\t|\n' -e '
            /[^\t]+\t[^\t]+\t\/\^.*\$\// {
                re=$0; sub(".*\t/\\^", "", re); sub("\\$/.*", "", re); gsub("(\\{|\\}).*$", "", re);
                out = out " %{" $2 " [" re "]} %{try %{ edit %{" $2 "}; exec %{/\\Q" re "<ret>vc} } catch %{ echo %{unable to find tag} } }"
            }
            /[^\t]+\t[^\t]+\t([0-9]+)/ { out = out " %{" $2 ":" $3 "} %{edit %{" $2 "} %{" $3 "}}" }
            END { print length(out) == 0 ? "echo -color Error no such tag " ENVIRON["tagname"] : "menu -auto-single " out }'
    }}

def tag-complete %{ eval -draft %{
    exec <space>hb<a-k>^\w+$<ret>
    %sh{ (
        compl=$(readtags -p "$kak_selection" | cut -f 1 | sort | uniq | sed -e 's/:/\\:/g' | sed -e 's/\n/:/g' )
        compl="${kak_cursor_line}.${kak_cursor_column}+${#kak_selection}@${kak_timestamp}:${compl}"
        echo "set buffer=$kak_bufname ctags_completions '${compl}'" | kak -p ${kak_session}
    ) > /dev/null 2>&1 < /dev/null & }
}}

def funcinfo %{
    eval -draft %{
        exec [(<space>B<a-k>[a-zA-Z_]+\(<ret>
        %sh{
            sigs=$(readtags -e ${kak_selection%(} | grep kind:f | sed -re 's/^(\S+).*(class|struct|namespace):(\S+).*signature:(.*)$/\4 [\3::\1]/')
            if [ -n "$sigs" ]; then
                echo "eval -client ${kak_client} %{info -anchor right '$sigs'}"
            fi
        }
    }
}

def ctags-enable-autoinfo %{
     hook window -group ctags-autoinfo NormalIdle .* funcinfo
     hook window -group ctags-autoinfo NormalEnd  .* info
     hook window -group ctags-autoinfo NormalKey  .* info
     hook window -group ctags-autoinfo InsertIdle .* funcinfo
}

def ctags-disable-autoinfo %{ rmhooks window ctags-autoinfo }

decl str ctagsopts "-R ."

def gentags -docstring 'generate tag file asynchronously' %{
    echo -color Information "launching tag generation in the background"
    %sh{ (
        if ctags -f .tags.kaktmp ${kak_opt_ctagsopts}; then
            mv .tags.kaktmp tags
            msg="tags generation complete"
        else
            msg="tags generation failed"
        fi
        echo "eval -client $kak_client echo -color Information '${msg}'" | kak -p ${kak_session}
    ) > /dev/null 2>&1 < /dev/null & }
}
