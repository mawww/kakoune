##
## base16.kak by lenormf
##

evaluate-commands %sh{
    scope="${1:-global}"

    black_lighterer='rgb:383838'
    black_lighter='rgb:2D2D2D'
    black_light='rgb:1C1C1C'
    cyan_light='rgb:7CB0FF'
    green_dark='rgb:A1B56C'
    grey_dark='rgb:585858'
    grey_light='rgb:D8D8D8'
    magenta_dark='rgb:AB4642'
    magenta_light='rgb:AB4434'
    orange_dark='rgb:DC9656'
    orange_light='rgb:F7CA88'
    purple_dark='rgb:BA8BAF'

    ## code
    echo "
        face ${scope} value ${orange_dark}+b
        face ${scope} type ${orange_light}
        face ${scope} variable ${magenta_dark}
        face ${scope} module ${green_dark}
        face ${scope} function ${cyan_light}
        face ${scope} string ${green_dark}
        face ${scope} keyword ${purple_dark}+b
        face ${scope} operator ${cyan_light}
        face ${scope} attribute ${orange_dark}
        face ${scope} comment ${grey_dark}
        face ${scope} meta ${orange_light}
        face ${scope} builtin default+b
    "

    ## markup
    echo "
        face ${scope} title blue
        face ${scope} header ${cyan_light}
        face ${scope} bold ${orange_light}
        face ${scope} italic ${orange_dark}
        face ${scope} mono ${green_dark}
        face ${scope} block ${orange_dark}
        face ${scope} link blue
        face ${scope} bullet ${magenta_light}
        face ${scope} list ${magenta_dark}
    "

    ## builtin
    echo "
        face ${scope} Default ${grey_light},${black_lighter}
        face ${scope} PrimarySelection white,blue+fg
        face ${scope} SecondarySelection black,blue+fg
        face ${scope} PrimaryCursor black,white+fg
        face ${scope} SecondaryCursor black,white+fg
        face ${scope} PrimaryCursorEol black,${cyan_light}+fg
        face ${scope} SecondaryCursorEol black,${cyan_light}+fg
        face ${scope} LineNumbers ${grey_light},${black_lighter}
        face ${scope} LineNumberCursor ${grey_light},rgb:282828+b
        face ${scope} MenuForeground ${grey_light},blue
        face ${scope} MenuBackground blue,${grey_light}
        face ${scope} MenuInfo ${cyan_light}
        face ${scope} Information ${black_light},${cyan_light}
        face ${scope} Error ${grey_light},${magenta_light}
        face ${scope} StatusLine ${grey_light},${black_lighterer}
        face ${scope} StatusLineMode ${orange_dark}
        face ${scope} StatusLineInfo ${cyan_light}
        face ${scope} StatusLineValue ${green_dark}
        face ${scope} StatusCursor ${black_lighterer},${cyan_light}
        face ${scope} Prompt ${black_light},${cyan_light}
        face ${scope} MatchingChar ${cyan_light},${black_light}+b
        face ${scope} BufferPadding ${cyan_light},${black_lighter}
        face ${scope} Whitespace ${grey_dark}+f
    "
}
