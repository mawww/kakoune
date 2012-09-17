hook global BufCreate .*\.(c|cc|cpp|cxx|C|h|hh|hpp|hxx|H) %{
    setb filetype cpp
}

hook global BufOpen .* %{ %sh{
     mimetype="$(file -b --mime-type ${kak_bufname})"
     if [[ "${mimetype}" == "text/x-c++" || "${mimetype}" == "text/x-c" ]]; then
         echo setb filetype cpp;
     fi
} }

hook global WinSetOption filetype=cpp %{
    addhl group cpp-highlight
    addhl -group cpp-highlight regex "\<(this|true|false|NULL|nullptr|)\>|\<-?\d+[fdiu]?|'((\\.)?|[^'\\])'" 0:value
    addhl -group cpp-highlight regex "\<(void|int|char|unsigned|float|bool|size_t)\>" 0:type
    addhl -group cpp-highlight regex "\<(while|for|if|else|do|switch|case|default|goto|break|continue|return|using|try|catch|throw|new|delete|and|or|not)\>" 0:keyword
    addhl -group cpp-highlight regex "\<(const|auto|namespace|inline|static|volatile|class|struct|enum|union|public|protected|private|template|typedef|virtual|friend|extern|typename)\>" 0:attribute
    addhl -group cpp-highlight regex "(?<!')\"(\\\"|[^\"])*\"" 0:string
    addhl -group cpp-highlight regex "(\`|(?<=\n))\h*#\h*[^\n]*" 0:macro
    addhl -group cpp-highlight regex "(//[^\n]*\n)|(/\*.*?(\*/|\'))" 0:comment
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
    exec ggi<c-r>%<ret><esc>ggxs\.<ret>c_<esc><space>A_INCLUDED<esc>ggxyppI#ifndef<space><esc>jI#define<space><esc>jI#endif<space>//<space><esc>O<esc>
}

def alt %{ %sh{
    shopt -s extglob
    case ${kak_bufname} in
         *.c|*.cc|*.cpp|*.cxx|*.C)
             altname=$(ls -1 "${kak_bufname%.*}".@(h|hh|hpp|hxx|H) 2> /dev/null | head -n 1)
         ;;
         *.h|*.hh|*.hpp|*.hxx|*.H)
             altname=$(ls -1 "${kak_bufname%.*}".@(c|cc|cpp|cxx|C) 2> /dev/null | head -n 1)
         ;;
    esac
    if [[ -e ${altname} ]]; then
       echo edit "'${altname}'"
    else
       echo echo "'alternative file not found'"
    fi
}}
