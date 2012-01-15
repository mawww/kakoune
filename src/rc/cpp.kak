hook WinCreate .*\.(c|cc|cpp|cxx|C|h|hh|hpp|hxx|H) \
    addhl group hlcpp; \
    addhl -group hlcpp regex "\<(true|false|NULL|nullptr)\>|\<-?\d+[fdiu]?|'((\\.)?|[^'\\])'" red default; \
    addhl -group hlcpp regex "\<(void|int|char|unsigned|float|bool|size_t)\>" yellow default; \
    addhl -group hlcpp regex "\<(while|for|if|else|do|switch|case|default|goto|break|continue|return|using|try|catch|throw)\>" blue default; \
    addhl -group hlcpp regex "\<(const|auto|namespace|static|volatile|class|struct|enum|union|public|protected|private|template|typedef|virtual)\>" green default; \
    addhl -group hlcpp regex "(?<!')\"(\\\"|[^\"])*\"" magenta default; \
    addhl -group hlcpp regex "(\`|(?<=\n))\h*#\h*[^\n]*" magenta default; \
    addhl -group hlcpp regex "(//[^\n]*\n)|(/\*.*?(\*/|\'))" cyan default; \
    addfilter preserve_indent; \
    addfilter cleanup_whitespaces
