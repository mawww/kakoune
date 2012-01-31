hook global WinCreate .*\.(c|cc|cpp|cxx|C|h|hh|hpp|hxx|H) \
    addhl group hlcpp; \
    addgrouphl hlcpp regex "\<(this|true|false|NULL|nullptr)\>|\<-?\d+[fdiu]?|'((\\.)?|[^'\\])'" red default; \
    addgrouphl hlcpp regex "\<(void|int|char|unsigned|float|bool|size_t)\>" yellow default; \
    addgrouphl hlcpp regex "\<(while|for|if|else|do|switch|case|default|goto|break|continue|return|using|try|catch|throw)\>" blue default; \
    addgrouphl hlcpp regex "\<(const|auto|namespace|static|volatile|class|struct|enum|union|public|protected|private|template|typedef|virtual|friend)\>" green default; \
    addgrouphl hlcpp regex "(?<!')\"(\\\"|[^\"])*\"" magenta default; \
    addgrouphl hlcpp regex "(\`|(?<=\n))\h*#\h*[^\n]*" magenta default; \
    addgrouphl hlcpp regex "(//[^\n]*\n)|(/\*.*?(\*/|\'))" cyan default; \
    addfilter preserve_indent; \
    addfilter cleanup_whitespaces; \
    hook window InsertEnd .* exec xs\h+(?=\n)<ret>d

hook global BufCreate .*\.(h|hh|hpp|hxx|H) \
    exec ggi<c-r>%<ret><esc>ggxs\.<ret>c_<esc><space>A_INCLUDED<esc>xyppI#ifndef<space><esc>jI#define<space><esc>jI#endif<space>//<space><esc>O<esc>

