# Greyscale: monochromatic grey-based light colorscheme

evaluate-commands %sh{
    grey_light_5="rgb:fafafa"
    grey_light_4="rgb:f5f5f5"
    grey_light_3="rgb:eeeeee"
    grey_light_2="rgb:e0e0e0"
    grey_light_1="rgb:bdbdbd"
    grey="rgb:9e9e9e"
    grey_dark_1="rgb:757575"
    grey_dark_2="rgb:616161"
    grey_dark_3="rgb:424242"
    grey_dark_4="rgb:212121"

    cat <<EOF

    # For Code
    set-face global keyword ${grey_dark_2}
    set-face global attribute ${grey_dark_2}
    set-face global type ${grey_dark_2}
    set-face global string ${grey_dark_1}
    set-face global value ${grey_dark_1}+b
    set-face global meta ${grey_dark_1}
    set-face global builtin ${grey}+b
    set-face global module ${grey_dark_1}
    set-face global comment ${grey}+i
    set-face global documentation comment
    set-face global function Default
    set-face global operator Default
    set-face global variable Default

    # For markup
    set-face global title ${grey_dark_2}+b
    set-face global header ${grey_dark_2}
    set-face global block ${grey_dark_1}
    set-face global mono ${grey_dark_1}
    set-face global link ${grey}+u
    set-face global list Default
    set-face global bullet +b

    # Built-in faces
    set-face global Default ${grey},${grey_light_2}
    set-face global PrimarySelection ${grey_light_3},${grey_dark_4}+fg
    set-face global SecondarySelection ${grey_light_2},${grey_dark_3}+fg
    set-face global PrimaryCursor ${grey_light_3},${grey_dark_1}+fg
    set-face global SecondaryCursor ${grey_light_3},${grey}+fg
    set-face global PrimaryCursorEol ${grey_light_1},${grey_dark_2}+fg
    set-face global SecondaryCursorEol ${grey_light_2},${grey_dark_1}+fg

    set-face global StatusLine ${grey_dark_3},${grey_light_1}
    set-face global StatusLineMode ${grey_light_2},${grey_dark_3}
    set-face global StatusLineInfo ${grey_light_2},${grey_dark_2}
    set-face global StatusLineValue ${grey_light_3},${grey_dark_2}+b
    set-face global StatusCursor ${grey_light_3},${grey}
    set-face global Prompt ${grey_light_2},${grey_dark_3}
    set-face global MenuForeground ${grey_light_4},${grey}
    set-face global MenuBackground ${grey_dark_2},${grey_light_3}
    set-face global MenuInfo ${grey}+i

    set-face global LineNumbers ${grey_light_5},${grey_dark_1}
    set-face global LineNumbersWrapped ${grey_light_2},${grey_dark_2}+i
    set-face global LineNumberCursor ${grey_light_2},${grey_dark_3}+b
    set-face global MatchingChar ${grey_dark_4},${grey_light_1}
    set-face global Whitespace ${grey_light_1}+f
    set-face global WrapMarker ${grey_light_1}+f

    set-face global Information ${grey_light_2},${grey_dark_2}
    set-face global Error ${grey_light_2},${grey_dark_3}
    set-face global DiagnosticError ${grey_dark_3}
    set-face global DiagnosticWarning ${grey_dark_2}
    set-face global BufferPadding ${grey_light_1}

EOF
}
