def -shell-params \
    -shell-completion %{ global -c $1 } \
    tag %{ %sh{
    if [[ $1 != "" ]]; then
       tagname=$1
    else
       tagname=${kak_selection}
    fi
    params=$(global --result grep ${tagname} | sed "s/\([^:]*\):\([0-9]*\):\(.*\)/'\1:\2 \3' 'edit \1 \2; try %{ exec \"20k41Xs\\\\Q\3<ret>\" } catch %{ echo \"could not find [\3] near \1:\2\"; exec \2g }'/")
    if [[ ${params} != "" ]]; then
       echo "menu -auto-single ${params//$'\n'/ }"
    else
       echo echo tag ${tagname} not found
    fi
}}
