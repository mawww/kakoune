##
## Red Phoenix (dkeg) - adapted by boj
##

evaluate-commands %sh{
    scope="${1:-global}"

    black="rgb:000000"
    blue="rgb:81a2be"

    orange1="rgb:F2361E"
    orange2="rgb:ED4B19"
    orange3="rgb:FA390F"
    light_orange1="rgb:DF9767"
    white1="rgb:EDEDED"
    white2="rgb:E1E1E1"
    gray1="rgb:6F6F6F"
    gray2="rgb:D1D1D1"
    gray3="rgb:2D2D2D"
    gray4="rgb:909090"
    tan1="rgb:D2C3AD"
    tan2="rgb:AAA998"
    tan3="rgb:DF9767"
    yellow1="rgb:AAA998"
    purple1="rgb:4C3A3D"

    foreground=${white1}
    background=${black}
    selection=${purple1}
    window=${gray3}
    text=${white2}
    text_light=${white1}
    line=${tan1}
    comment=${gray1}

    ## code
    echo "
        face ${scope} value ${orange2}
        face ${scope} type ${gray2}
        face ${scope} variable ${orange1}
        face ${scope} module ${gray2}
        face ${scope} function ${yellow1}
        face ${scope} string ${tan2}
        face ${scope} keyword ${light_orange1}
        face ${scope} operator ${yellow1}
        face ${scope} attribute ${tan1}
        face ${scope} comment ${gray1}
        face ${scope} meta ${gray2}
        face ${scope} builtin ${tan1}
    "

    ## markup
    echo "
        face ${scope} title blue
        face ${scope} header ${orange1}
        face ${scope} bold ${orange2}
        face ${scope} italic ${orange3}
        face ${scope} mono ${yellow1}
        face ${scope} block ${tan1}
        face ${scope} link blue
        face ${scope} bullet ${gray1}
        face ${scope} list ${gray1}
    "

    ## builtin
    echo "
        face ${scope} Default ${text},${background}
        face ${scope} PrimarySelection default,${selection}+fg
        face ${scope} SecondarySelection default,${selection}+fg
        face ${scope} PrimaryCursor black,${tan1}+fg
        face ${scope} SecondaryCursor black,${tan2}+fg
        face ${scope} PrimaryCursorEol black,${orange1}+fg
        face ${scope} SecondaryCursorEol black,${orange2}+fg
        face ${scope} LineNumbers ${text_light},${background}
        face ${scope} LineNumberCursor ${text},${gray1}+b
        face ${scope} MenuForeground ${text_light},blue
        face ${scope} MenuBackground ${orange1},${window}
        face ${scope} MenuInfo ${gray1}
        face ${scope} Information white,${window}
        face ${scope} Error white,${gray1}
        face ${scope} StatusLine ${text},${window}
        face ${scope} StatusLineMode ${yellow1}+b
        face ${scope} StatusLineInfo ${orange2}
        face ${scope} StatusLineValue ${orange2}
        face ${scope} StatusCursor ${window},${orange2}
        face ${scope} Prompt ${background},${orange2}
        face ${scope} MatchingChar ${orange3},${background}+b
        face ${scope} BufferPadding ${orange2},${background}
        face ${scope} Whitespace default+f
    "
}
