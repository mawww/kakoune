hook global BufCreate .+\.cmake|.*/CMakeLists.txt %{
    set-option buffer filetype cmake
}

hook global BufCreate .*/CMakeCache.txt %{
    set-option buffer filetype ini
}

hook global WinSetOption filetype=cmake %{
    require-module cmake
}

hook -group cmake-highlight global WinSetOption filetype=cmake %{
    add-highlighter window/cmake ref cmake
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/cmake }
}

provide-module cmake %{

add-highlighter shared/cmake regions
add-highlighter shared/cmake/code default-region group
add-highlighter shared/cmake/comment  region '#' '$' fill comment
add-highlighter shared/cmake/argument region -recurse '\(' '\w+\h*\(\K' '(?=\))' regions

add-highlighter shared/cmake/code/ regex '\w+\h*(?=\()' 0:meta

add-highlighter shared/cmake/argument/args default-region regex '\$\{\w+\}' 0:variable
add-highlighter shared/cmake/argument/comment  region '#' '$' fill comment
add-highlighter shared/cmake/argument/quoted region '"' '(?<!\\)(\\\\)*"' group
add-highlighter shared/cmake/argument/raw-quoted region -match-capture '\[(=*)\[' '\](=*)\]' ref cmake/argument/quoted

add-highlighter shared/cmake/argument/quoted/ fill string
add-highlighter shared/cmake/argument/quoted/ regex '\$\{\w+\}' 0:variable
add-highlighter shared/cmake/argument/quoted/ regex '\w+\h*(?=\()' 0:function

}
