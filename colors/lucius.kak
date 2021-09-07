# lucius theme

evaluate-commands %sh{
    # first we define the lucius colors as named colors
    lucius_darker_grey="rgb:303030"
    lucius_dark_grey="rgb:444444"
    lucius_grey="rgb:808080"
    lucius_light_grey="rgb:b2b2b2"
    lucius_lighter_grey="rgb:d7d7d7"

    lucius_dark_red="rgb:870000"
    lucius_light_red="rgb:ff8787"
    lucius_orange="rgb:d78700"
    lucius_purple="rgb:d7afd7"

    lucius_dark_green="rgb:5f875f"
    lucius_bright_green="rgb:87af00"
    lucius_green="rgb:afd787"
    lucius_light_green="rgb:d7d7af"

    lucius_dark_blue="rgb:005f87"
    lucius_blue="rgb:87afd7"
    lucius_light_blue="rgb:87d7ff"

    echo "
        # then we map them to code
        face global value ${lucius_light_green}
        face global type ${lucius_blue}
        face global variable ${lucius_green}
        face global module ${lucius_green}
        face global function ${lucius_light_blue}
        face global string ${lucius_light_green}
        face global keyword ${lucius_light_blue}
        face global operator ${lucius_green}
        face global attribute ${lucius_light_blue}
        face global comment ${lucius_grey}
        face global documentation comment
        face global meta ${lucius_purple}
        face global builtin default+b

        # and markup
        face global title ${lucius_light_blue}
        face global header ${lucius_light_green}
        face global mono ${lucius_light_green}
        face global block ${lucius_light_blue}
        face global link ${lucius_light_green}
        face global bullet ${lucius_green}
        face global list ${lucius_blue}

        # and built in faces
        face global Default ${lucius_lighter_grey},${lucius_darker_grey}
        face global PrimarySelection ${lucius_darker_grey},${lucius_orange}+fg
        face global SecondarySelection  ${lucius_lighter_grey},${lucius_dark_blue}+fg
        face global PrimaryCursor ${lucius_darker_grey},${lucius_lighter_grey}+fg
        face global SecondaryCursor ${lucius_darker_grey},${lucius_lighter_grey}+fg
        face global PrimaryCursorEol ${lucius_darker_grey},${lucius_dark_green}+fg
        face global SecondaryCursorEol ${lucius_darker_grey},${lucius_dark_green}+fg
        face global LineNumbers ${lucius_grey},${lucius_dark_grey}
        face global LineNumberCursor ${lucius_grey},${lucius_dark_grey}+b
        face global MenuForeground ${lucius_blue},${lucius_dark_blue}
        face global MenuBackground ${lucius_darker_grey},${lucius_light_grey}
        face global MenuInfo ${lucius_grey}
        face global Information ${lucius_lighter_grey},${lucius_dark_green}
        face global Error ${lucius_light_red},${lucius_dark_red}
        face global DiagnosticError ${lucius_light_red}
        face global DiagnosticWarning ${lucius_purple}
        face global StatusLine ${lucius_lighter_grey},${lucius_dark_grey}
        face global StatusLineMode ${lucius_lighter_grey},${lucius_dark_green}+b
        face global StatusLineInfo ${lucius_dark_grey},${lucius_lighter_grey}
        face global StatusLineValue ${lucius_lighter_grey}
        face global StatusCursor default,${lucius_blue}
        face global Prompt ${lucius_lighter_grey}
        face global MatchingChar ${lucius_lighter_grey},${lucius_bright_green}
        face global BufferPadding ${lucius_green},${lucius_darker_grey}
        face global Whitespace ${lucius_grey}+f
    "
}
