hook WinCreate (.*/)?(kakrc|.*.kak) \
    addhl group hlkakrc; \
    addhl -group hlkakrc regex \<(hook|addhl|rmhl|addfilter|rmfilter|exec|source|runtime)\> green default; \
    addhl -group hlkakrc regex \<(default|black|red|green|yellow|blue|magenta|cyan|white)\> yellow default; \
    addhl -group hlkakrc regex (?<=\<hook)\h+\w+\h+\H+ magenta default; \
    addhl -group hlkakrc regex (?<=\<hook)\h+\w+ cyan default; \
    addhl -group hlkakrc regex (?<=\<regex)\h+\H+ magenta default
