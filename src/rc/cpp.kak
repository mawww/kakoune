hook global BufCreate .*\.(c|cc|cpp|cxx|C|h|hh|hpp|hxx|H) %{
    setb filetype cpp
}

hook global BufOpen .* %{ %sh{
     mimetype="$(file -b --mime-type ${kak_bufname})"
     if [[ "${mimetype}" == "text/x-c++" || "${mimetype}" == "text/x-c" ]]; then
         echo setb filetype cpp;
     fi
} }

hook global WinSetOption filetype=cpp %~
    addhl group cpp-highlight
    addhl -group cpp-highlight regex "\<(this|true|false|NULL|nullptr|)\>|\<-?\d+[fdiu]?|'((\\.)?|[^'\\])'" 0:value
    addhl -group cpp-highlight regex "\<(void|int|char|unsigned|float|bool|size_t)\>" 0:type
    addhl -group cpp-highlight regex "\<(while|for|if|else|do|switch|case|default|goto|break|continue|return|using|try|catch|throw|new|delete|and|or|not|operator|explicit)\>" 0:keyword
    addhl -group cpp-highlight regex "\<(const|mutable|auto|namespace|inline|static|volatile|class|struct|enum|union|public|protected|private|template|typedef|virtual|friend|extern|typename|override|final)\>" 0:attribute
    addhl -group cpp-highlight regex "^\h*?#.*?(?<!\\)$" 0:macro
    addhl -group cpp-highlight regex "(?<!')\".*?(?<!\\)(\\\\)*\"" 0:string
    addhl -group cpp-highlight regex "(//[^\n]*\n)|(/\*.*?(\*/|\'))" 0:comment
    addfilter group cpp-filters
    addfilter -group cpp-filters regex ^(\h+)([^\n]*[^([{]\h*|$) \n \n$1
    addfilter -group cpp-filters regex ^(\h*)[^\n]*[([{]\h* \n '\n$1    '
    addfilter -group cpp-filters cleanup_whitespaces
    hook window InsertEnd .* -id cpp-hooks %{ exec -draft <a-x>s\h+$<ret>d }
~

hook global WinSetOption filetype=(?!cpp).* %{
    rmhl cpp-highlight
    rmfilter cpp-filters
    rmhooks window cpp-hooks
}

hook global BufNew .*\.(h|hh|hpp|hxx|H) %{
    exec ggi<c-r>%<ret><esc>ggxs\.<ret>c_<esc><space>A_INCLUDED<esc>ggxyppI#ifndef<space><esc>jI#define<space><esc>jI#endif<space>//<space><esc>O<esc>
}

decl str-list alt_dirs ".;.."

def alt %{ %sh{
    shopt -s extglob
    alt_dirs=${kak_opt_alt_dirs//,/ }
    file=$(basename ${kak_bufname})
    dir=$(dirname ${kak_bufname})

    case ${file} in
         *.c|*.cc|*.cpp|*.cxx|*.C)
             for alt_dir in ${alt_dirs}; do
                 altname=$(ls -1 "${dir}/${alt_dir}/${file%.*}".@(h|hh|hpp|hxx|H) 2> /dev/null | head -n 1)
                 [[ -e ${altname} ]] && break
             done
         ;;
         *.h|*.hh|*.hpp|*.hxx|*.H)
             for alt_dir in ${alt_dirs}; do
                 altname=$(ls -1 "${dir}/${alt_dir}/${file%.*}".@(c|cc|cpp|cxx|C) 2> /dev/null | head -n 1)
                 [[ -e ${altname} ]] && break
             done
         ;;
    esac
    if [[ -e ${altname} ]]; then
       echo edit "'${altname}'"
    else
       echo echo "'alternative file not found'"
    fi
}}
