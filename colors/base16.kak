##
## base16.kak by lenormf
##

%sh{
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
        face value ${orange_dark}+b
        face type ${orange_light}
        face variable ${magenta_dark}
        face module ${green_dark}
        face function ${cyan_light}
        face string ${green_dark}
        face keyword ${purple_dark}+b
        face operator ${cyan_light}
        face attribute ${orange_dark}
        face comment ${grey_dark}
        face meta ${orange_light}
        face builtin default+b
    "

    ## markup
    echo "
        face title blue
        face header ${cyan_light}
        face bold ${orange_light}
        face italic ${orange_dark}
        face mono ${green_dark}
        face block ${orange_dark}
        face link blue
        face bullet ${magenta_light}
        face list ${magenta_dark}
    "

    ## builtin
    echo "
        face Default ${grey_light},${black_lighter}
        face PrimarySelection white,blue
        face SecondarySelection black,blue
        face PrimaryCursor black,white
        face SecondaryCursor black,white
        face LineNumbers ${grey_light},${black_lighter}
        face LineNumberCursor ${grey_light},rgb:282828+b
        face MenuForeground ${grey_light},blue
        face MenuBackground blue,${grey_light}
        face MenuInfo ${cyan_light}
        face Information ${black_light},${cyan_light}
        face Error ${grey_light},${magenta_light}
        face StatusLine ${grey_light},${black_lighterer}
        face StatusLineMode ${orange_dark}
        face StatusLineInfo ${cyan_light}
        face StatusLineValue ${green_dark}
        face StatusCursor ${black_lighterer},${cyan_light}
        face Prompt ${black_light},${cyan_light}
        face MatchingChar ${cyan_light},${black_light}+b
        face BufferPadding ${cyan_light},${black_lighter}
    "
}
