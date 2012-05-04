hook global WinCreate .*\.(c|cc|cpp|cxx|C|h|hh|hpp|hxx|H) \
    addhl group hlcpp; \
    addhl -group hlcpp regex "\<(this|true|false|NULL|nullptr)\>|\<-?\d+[fdiu]?|'((\\.)?|[^'\\])'" red default; \
    addhl -group hlcpp regex "\<(void|int|char|unsigned|float|bool|size_t)\>" yellow default; \
    addhl -group hlcpp regex "\<(while|for|if|else|do|switch|case|default|goto|break|continue|return|using|try|catch|throw)\>" blue default; \
    addhl -group hlcpp regex "\<(const|auto|namespace|static|volatile|class|struct|enum|union|public|protected|private|template|typedef|virtual|friend)\>" green default; \
    addhl -group hlcpp regex "(?<!')\"(\\\"|[^\"])*\"" magenta default; \
    addhl -group hlcpp regex "(\`|(?<=\n))\h*#\h*[^\n]*" magenta default; \
    addhl -group hlcpp regex "(//[^\n]*\n)|(/\*.*?(\*/|\'))" cyan default; \
    addfilter preserve_indent; \
    addfilter cleanup_whitespaces; \
    hook window InsertEnd .* exec xs\h+(?=\n)<ret>d

hook global BufCreate .*\.(h|hh|hpp|hxx|H) \
    exec ggi<c-r>%<ret><esc>ggxs\.<ret>c_<esc><space>A_INCLUDED<esc>xyppI#ifndef<space><esc>jI#define<space><esc>jI#endif<space>//<space><esc>O<esc>

def alt edit \
    `case ${kak_bufname} in
         *.c) echo ${kak_bufname/%c/h} ;;
         *.cc) echo ${kak_bufname/%cc/hh} ;;
         *.cpp) echo ${kak_bufname/%cpp/hpp} ;;
         *.cxx) echo ${kak_bufname/%cxx/hxx} ;;
         *.C) echo ${kak_bufname/%C/H} ;;
         *.h) echo ${kak_bufname/%h/c} ;;
         *.hh) echo ${kak_bufname/%hh/cc} ;;
         *.hpp) echo ${kak_bufname/%hpp/cpp} ;;
         *.hxx) echo ${kak_bufname/%hxx/cxx} ;;
         *.H) echo ${kak_bufname/%H/C} ;;
    esac` 