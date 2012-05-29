def -env-params \
    -shell-completion 'global -c ${kak_param0}' \
    tag eval \
   `if [[ ${kak_param0} != "" ]]; then
       tagname=${kak_param0}
    else
       tagname=${kak_selection}
    fi
    params=$(global --result grep ${tagname} | sed 's/\([^:]*\):\([0-9]*\):\(.*\)/"\1:\2 \3" "edit \1 \2"/')
    if [[ ${params} != "" ]]; then
       echo "menu -auto-single $params"
    else
       echo echo tag ${tagname} not found
    fi`
