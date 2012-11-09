# Kakoune Exuberant CTags support script
#
# This script requires the readtags command available in ctags source but
# not installed by default

def -shell-params \
    -shell-completion 'readtags -p "$1" | cut -f 1 | sort | uniq' \
    tag \
    %{ %sh{
        if [[ -z "$1" ]]; then tagname=${kak_selection}; else tagname=$1; fi
        matches=$(readtags ${tagname})
        if [[ -z "${matches}" ]]; then
            echo "echo tag not found ${tagname}"
        else
            menuparam=$(readtags ${tagname} | perl -i -ne '
                /([^\t]+)\t([^\t]+)\t\/\^(.*)\$\// and print "%{$2 [$3]} %{try %{ edit %{$2}; exec %{%s\\Q$3<ret>} } catch %{ echo %{unable to find tag} } } ";
                /([^\t]+)\t([^\t]+)\t(\d+)/        and print "%{$2:$3} %{edit %{$2}; exec %{$3g}} ";
            ')

            if [[ -z "${menuparam}" ]]; then
                echo "echo no such tag ${tagname}";
            else
                echo "menu -auto-single ${menuparam//$'\n'/ }";
            fi
        fi
    }}
