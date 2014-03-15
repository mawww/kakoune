# Kakoune Exuberant CTags support script
#
# This script requires the readtags command available in ctags source but
# not installed by default

def -shell-params \
    -shell-completion 'readtags -p "$1" | cut -f 1 | sort | uniq' \
    -docstring 'jump to tag definition' \
    tag \
    %{ %sh{
        if [ -z "$1" ]; then tagname=${kak_selection}; else tagname="$1"; fi
        matches=$(readtags ${tagname})
        if [ -z "${matches}" ]; then
            echo "echo tag not found ${tagname}"
        else
            menuparam=$(readtags ${tagname} | perl -i -ne '
                /([^\t]+)\t([^\t]+)\t\/\^([^{}]*).*\$\// and print "%{$2 [$3]} %{try %{ edit %{$2}; exec %{/\\Q$3<ret>vc} } catch %{ echo %{unable to find tag} } } ";
                /([^\t]+)\t([^\t]+)\t(\d+)/              and print "%{$2:$3} %{edit %{$2} %{$3}}";
            ' | sed -e 's/\n/ /g')

            if [ -z "${menuparam}" ]; then
                echo "echo no such tag ${tagname}";
            else
                echo "menu -auto-single ${menuparam}";
            fi
        fi
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
     hook window -id ctags-autoinfo NormalIdle .* funcinfo
     hook window -id ctags-autoinfo NormalEnd  .* info
     hook window -id ctags-autoinfo NormalKey  .* info
     hook window -id ctags-autoinfo InsertIdle .* funcinfo
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
