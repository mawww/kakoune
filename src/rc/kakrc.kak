hook WinCreate (.*/)?(kakrc|.*.kak) \
    addhl group hlkakrc; \
    addgrouphl hlkakrc regex \<(hook|addhl|rmhl|addgrouphl|rmgrouphl|addfilter|rmfilter|exec|source|runtime)\> green default; \
    addgrouphl hlkakrc regex \<(default|black|red|green|yellow|blue|magenta|cyan|white)\> yellow default; \
    addgrouphl hlkakrc regex (?<=\<hook)\h+\w+\h+\H+ magenta default; \
    addgrouphl hlkakrc regex (?<=\<hook)\h+\w+ cyan default; \
    addgrouphl hlkakrc regex (?<=\<regex)\h+\H+ magenta default
