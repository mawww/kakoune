##
## base16.kak by lenormf
##

evaluate-commands %sh{
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
        face global value ${orange_dark}+b
        face global type ${orange_light}
        face global variable ${magenta_dark}
        face global module ${green_dark}
        face global function ${cyan_light}
        face global string ${green_dark}
        face global keyword ${purple_dark}+b
        face global operator ${cyan_light}
        face global attribute ${orange_dark}
        face global comment ${grey_dark}
        face global documentation comment
        face global meta ${orange_light}
        face global builtin default+b
    "

    ## markup
    echo "
        face global title blue
        face global header ${cyan_light}
        face global mono ${green_dark}
        face global block ${orange_dark}
        face global link blue
        face global bullet ${magenta_light}
        face global list ${magenta_dark}
    "

    ## builtin
    echo "
        face global Default ${grey_light},${black_lighter}
        face global PrimarySelection white,blue+fg
        face global SecondarySelection black,blue+fg
        face global PrimaryCursor black,white+fg
        face global SecondaryCursor black,white+fg
        face global PrimaryCursorEol black,${cyan_light}+fg
        face global SecondaryCursorEol black,${cyan_light}+fg
        face global LineNumbers ${grey_light},${black_lighter}
        face global LineNumberCursor ${grey_light},rgb:282828+b
        face global MenuForeground ${grey_light},blue
        face global MenuBackground blue,${grey_light}
        face global MenuInfo ${cyan_light}
        face global Information ${black_light},${cyan_light}
        face global Error ${grey_light},${magenta_light}
        face global DiagnosticError ${magenta_light}
        face global DiagnosticWarning ${cyan_light}
        face global StatusLine ${grey_light},${black_lighterer}
        face global StatusLineMode ${orange_dark}
        face global StatusLineInfo ${cyan_light}
        face global StatusLineValue ${green_dark}
        face global StatusCursor ${black_lighterer},${cyan_light}
        face global Prompt ${black_light},${cyan_light}
        face global MatchingChar ${cyan_light},${black_light}+b
        face global BufferPadding ${cyan_light},${black_lighter}
        face global Whitespace ${grey_dark}+f
    "
}
