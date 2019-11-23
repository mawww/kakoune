# lucius theme

evaluate-commands %sh{
    scope="${1:-global}"

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
        face ${scope} value ${lucius_light_green}
        face ${scope} type ${lucius_blue}
        face ${scope} variable ${lucius_green}
        face ${scope} module ${lucius_green}
        face ${scope} function ${lucius_light_blue}
        face ${scope} string ${lucius_light_green}
        face ${scope} keyword ${lucius_light_blue}
        face ${scope} operator ${lucius_green}
        face ${scope} attribute ${lucius_light_blue}
        face ${scope} comment ${lucius_grey}
        face ${scope} meta ${lucius_purple}
        face ${scope} builtin default+b

        # and markup
        face ${scope} title ${lucius_light_blue}
        face ${scope} header ${lucius_light_green}
        face ${scope} bold ${lucius_blue}
        face ${scope} italic ${lucius_green}
        face ${scope} mono ${lucius_light_green}
        face ${scope} block ${lucius_light_blue}
        face ${scope} link ${lucius_light_green}
        face ${scope} bullet ${lucius_green}
        face ${scope} list ${lucius_blue}

        # and built in faces
        face ${scope} Default ${lucius_lighter_grey},${lucius_darker_grey}
        face ${scope} PrimarySelection ${lucius_darker_grey},${lucius_orange}+fg
        face ${scope} SecondarySelection  ${lucius_lighter_grey},${lucius_dark_blue}+fg
        face ${scope} PrimaryCursor ${lucius_darker_grey},${lucius_lighter_grey}+fg
        face ${scope} SecondaryCursor ${lucius_darker_grey},${lucius_lighter_grey}+fg
        face ${scope} PrimaryCursorEol ${lucius_darker_grey},${lucius_dark_green}+fg
        face ${scope} SecondaryCursorEol ${lucius_darker_grey},${lucius_dark_green}+fg
        face ${scope} LineNumbers ${lucius_grey},${lucius_dark_grey}
        face ${scope} LineNumberCursor ${lucius_grey},${lucius_dark_grey}+b
        face ${scope} MenuForeground ${lucius_blue},${lucius_dark_blue}
        face ${scope} MenuBackground ${lucius_darker_grey},${lucius_light_grey}
        face ${scope} MenuInfo ${lucius_grey}
        face ${scope} Information ${lucius_lighter_grey},${lucius_dark_green}
        face ${scope} Error ${lucius_light_red},${lucius_dark_red}
        face ${scope} StatusLine ${lucius_lighter_grey},${lucius_dark_grey}
        face ${scope} StatusLineMode ${lucius_lighter_grey},${lucius_dark_green}+b
        face ${scope} StatusLineInfo ${lucius_dark_grey},${lucius_lighter_grey}
        face ${scope} StatusLineValue ${lucius_lighter_grey}
        face ${scope} StatusCursor default,${lucius_blue}
        face ${scope} Prompt ${lucius_lighter_grey}
        face ${scope} MatchingChar ${lucius_lighter_grey},${lucius_bright_green}
        face ${scope} BufferPadding ${lucius_green},${lucius_darker_grey}
        face ${scope} Whitespace ${lucius_grey}+f
    "
}
