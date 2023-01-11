##
## Red Phoenix (dkeg) - adapted by boj
##

evaluate-commands %sh{
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
        face global value ${orange2}
        face global type ${gray2}
        face global variable ${orange1}
        face global module ${gray2}
        face global function ${yellow1}
        face global string ${tan2}
        face global keyword ${light_orange1}
        face global operator ${yellow1}
        face global attribute ${tan1}
        face global comment ${gray1}
        face global documentation comment
        face global meta ${gray2}
        face global builtin ${tan1}
    "

    ## markup
    echo "
        face global title blue
        face global header ${orange1}
        face global mono ${yellow1}
        face global block ${tan1}
        face global link blue
        face global bullet ${gray1}
        face global list ${gray1}
    "

    ## builtin
    echo "
        face global Default ${text},${background}
        face global PrimarySelection default,${selection}+fg
        face global SecondarySelection default,${selection}+fg
        face global PrimaryCursor black,${tan1}+fg
        face global SecondaryCursor black,${tan2}+fg
        face global PrimaryCursorEol black,${orange1}+fg
        face global SecondaryCursorEol black,${orange2}+fg
        face global LineNumbers ${text_light},${background}
        face global LineNumberCursor ${text},${gray1}+b
        face global MenuForeground ${text_light},blue
        face global MenuBackground ${orange1},${window}
        face global MenuInfo ${gray1}
        face global Information white,${window}
        face global Error white,${gray1}
        face global DiagnosticError ${orange1}
        face global DiagnosticWarning ${orange2}
        face global StatusLine ${text},${window}
        face global StatusLineMode ${yellow1}+b
        face global StatusLineInfo ${orange2}
        face global StatusLineValue ${orange2}
        face global StatusCursor ${window},${orange2}
        face global Prompt ${background},${orange2}
        face global MatchingChar ${orange3},${background}+b
        face global BufferPadding ${orange2},${background}
        face global Whitespace default+f
    "
}
