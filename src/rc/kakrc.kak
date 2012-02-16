hook global WinCreate (.*/)?(kakrc|.*.kak) \
    addhl group hlkakrc; \
    addgrouphl hlkakrc regex \<(hook|addhl|rmhl|addgrouphl|rmgrouphl|addfilter|rmfilter|exec|source|runtime|def|echo|edit)\> green default; \
    addgrouphl hlkakrc regex \<(default|black|red|green|yellow|blue|magenta|cyan|white)\> yellow default; \
    addgrouphl hlkakrc regex (?<=\<hook)(\h+\w+){2}\h+\H+ magenta default; \
    addgrouphl hlkakrc regex (?<=\<hook)(\h+\w+){2} cyan default; \
    addgrouphl hlkakrc regex (?<=\<hook)(\h+\w+) red default; \
    addgrouphl hlkakrc regex (?<=\<hook)(\h+(global|window)) blue default; \
    addgrouphl hlkakrc regex (?<=\<regex)\h+\H+ magenta default
