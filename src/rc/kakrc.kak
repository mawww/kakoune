hook global WinCreate (.*/)?(kakrc|.*.kak) \
    addhl group hlkakrc; \
    addhl -group hlkakrc regex \<(hook|addhl|rmhl|addfilter|rmfilter|exec|source|runtime|def|echo|edit)\> green default; \
    addhl -group hlkakrc regex \<(default|black|red|green|yellow|blue|magenta|cyan|white)\> yellow default; \
    addhl -group hlkakrc regex (?<=\<hook)(\h+\w+){2}\h+\H+ magenta default; \
    addhl -group hlkakrc regex (?<=\<hook)(\h+\w+){2} cyan default; \
    addhl -group hlkakrc regex (?<=\<hook)(\h+\w+) red default; \
    addhl -group hlkakrc regex (?<=\<hook)(\h+(global|window)) blue default; \
    addhl -group hlkakrc regex (?<=\<regex)\h+\H+ magenta default
