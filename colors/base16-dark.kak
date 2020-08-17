##
## base16-dark theme by chriskempson, adapted for kakoune by clarfon
##

evaluate-commands %sh{
    base00="rgb:181818"
    base01="rgb:282828"
    base02="rgb:383838"
    base03="rgb:585858"
    base04="rgb:b8b8b8"
    base05="rgb:d8d8d8"
    base06="rgb:e8e8e8"
    base07="rgb:f8f8f8"
    base08="rgb:ab4642"
    base09="rgb:dc9656"
    base0a="rgb:f7ca88"
    base0b="rgb:a1b56c"
    base0c="rgb:86c1b9"
    base0d="rgb:7cafc2"
    base0e="rgb:ba8baf"
    base0f="rgb:a16946"

    ## code
    echo "
        face global value ${base09}+b
        face global type ${base0a}
        face global variable ${base08}
        face global module ${base0b}
        face global function ${base0d}
        face global string ${base0b}
        face global keyword ${base0e}+b
        face global operator ${base05}
        face global attribute ${base0e}
        face global comment ${base04}
        face global documentation comment
        face global meta ${base0c}
        face global builtin default+b
    "

    ## markup
    echo "
        face global title ${base0d}
        face global header ${base0d}
        face global mono ${base0b}
        face global block ${base0b}
        face global link ${base09}
        face global bullet ${base05}
        face global list ${base08}
    "

    ## builtin
    echo "
        face global Default ${base05},${base00}
        face global PrimarySelection ${base0c},${base02}+fg
        face global SecondarySelection ${base0d},${base01}+fg
        face global PrimaryCursor ${base02},${base0c}+fg
        face global SecondaryCursor ${base01},${base0d}+fg
        face global PrimaryCursorEol ${base02},${base0c}+fg
        face global SecondaryCursorEol ${base01},${base0d}+fg
        face global LineNumbers ${base04},${base01}
        face global LineNumberCursor ${base01},${base03}+b
        face global MenuForeground ${base0c},${base01}
        face global MenuBackground ${base01},${base0c}
        face global MenuInfo ${base01}
        face global Information ${base01},${base0a}
        face global Error ${base07},${base08}
        face global StatusLine ${base04},${base01}
        face global StatusLineMode ${base0b}
        face global StatusLineInfo ${base0c}
        face global StatusLineValue ${base09}
        face global StatusCursor ${base02},${base0c}
        face global Prompt ${base0a},${base00}
        face global MatchingChar ${base00},${base0f}+b
        face global BufferPadding ${base03},${base01}
        face global Whitespace ${base03}+f
    "
}
