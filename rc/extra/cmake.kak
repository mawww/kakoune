hook global BufCreate .+\.cmake|.*/CMakeLists.txt %{
    set-option buffer filetype cmake
}

add-highlighter shared/ regions -match-capture -default code cmake \
   comment '#' '$' '' \
   argument '\w+\h*\(\K' '(?=\))' '\(' \

add-highlighter shared/cmake/argument regions -match-capture -default args argument \
   quoted '"' '(?<!\\)(\\\\)*"' '' \
   quoted '\[(=*)\[' '\](=*)\]' ''

add-highlighter shared/cmake/comment fill comment
add-highlighter shared/cmake/code regex '\w+\h*(?=\()' 0:meta

add-highlighter shared/cmake/argument/argument/args regex '\$\{\w+\}' 0:variable
add-highlighter shared/cmake/argument/argument/quoted fill string
add-highlighter shared/cmake/argument/argument/quoted regex '\$\{\w+\}' 0:variable
add-highlighter shared/cmake/argument/argument/quoted regex '\w+\h*(?=\()' 0:function

hook -group cmake-highlight global WinSetOption filetype=cmake %{ add-highlighter window ref cmake }
hook -group cmake-highlight global WinSetOption filetype=(?!cmake).* %{ remove-highlighter window/cmake }
