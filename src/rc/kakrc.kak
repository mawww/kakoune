hook WinCreate (.*/)?(kakrc|.*.kak) \
    addhl group hlkakrc; \
    addhl -group hlkakrc regex (hook|addhl|rmhl|addfilter|rmfilter|exec) green default; \
    addhl -group hlkakrc regex (?<=hook)\h+\w+\h+\H+ magenta default; \
    addhl -group hlkakrc regex (?<=hook)\h+\w+ cyan default
