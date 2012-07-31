hook global BufCreate .*\.(c|cc|cpp|cxx|C|h|hh|hpp|hxx|H) %{
    setb filetype cpp
}

hook global WinSetOption filetype=cpp %{
    addhl group cpp-highlight;
    addhl -group cpp-highlight regex "\<(this|true|false|NULL|nullptr|)\>|\<-?\d+[fdiu]?|'((\\.)?|[^'\\])'" red default;
    addhl -group cpp-highlight regex "\<(void|int|char|unsigned|float|bool|size_t)\>" yellow default;
    addhl -group cpp-highlight regex "\<(while|for|if|else|do|switch|case|default|goto|break|continue|return|using|try|catch|throw|new|delete|and|or|not)\>" blue default;
    addhl -group cpp-highlight regex "\<(const|auto|namespace|inline|static|volatile|class|struct|enum|union|public|protected|private|template|typedef|virtual|friend|extern|typename)\>" green default;
    addhl -group cpp-highlight regex "(?<!')\"(\\\"|[^\"])*\"" magenta default;
    addhl -group cpp-highlight regex "(\`|(?<=\n))\h*#\h*[^\n]*" magenta default;
    addhl -group cpp-highlight regex "(//[^\n]*\n)|(/\*.*?(\*/|\'))" cyan default;
    addfilter group cpp-filters;
    addfilter -group cpp-filters preserve_indent;
    addfilter -group cpp-filters cleanup_whitespaces;
    hook window InsertEnd .* %{ exec xs\h+(?=\n)<ret>d }
}

hook global WinSetOption filetype=(?!cpp).* %{
    rmhl cpp-highlight;
    rmfilter cpp-filters
}

hook global BufNew .*\.(h|hh|hpp|hxx|H) %{
    exec ggi<c-r>%<ret><esc>ggxs\.<ret>c_<esc><space>A_INCLUDED<esc>xyppI#ifndef<space><esc>jI#define<space><esc>jI#endif<space>//<space><esc>O<esc>
}

def alt %{ edit %sh{
    case ${kak_bufname} in
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
    esac
}} 