# lucius theme

%sh{
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
        face value ${lucius_light_green}
        face type ${lucius_blue}
        face variable ${lucius_green}
        face module ${lucius_green}
        face function ${lucius_light_blue}
        face string ${lucius_light_green}
        face keyword ${lucius_light_blue}
        face operator ${lucius_green}
        face attribute ${lucius_light_blue}
        face comment ${lucius_grey}
        face meta ${lucius_purple}
        face builtin default+b

        # and markup
        face title ${lucius_light_blue}
        face header ${lucius_light_green}
        face bold ${lucius_blue}
        face italic ${lucius_green}
        face mono ${lucius_light_green}
        face block ${lucius_light_blue}
        face link ${lucius_light_green}
        face bullet ${lucius_green}
        face list ${lucius_blue}

        # and built in faces
        face Default ${lucius_lighter_grey},${lucius_darker_grey}
        face PrimarySelection ${lucius_darker_grey},${lucius_orange}
        face SecondarySelection  ${lucius_lighter_grey},${lucius_dark_blue}
        face PrimaryCursor ${lucius_darker_grey},${lucius_lighter_grey}
        face SecondaryCursor ${lucius_darker_grey},${lucius_lighter_grey}
        face LineNumbers ${lucius_grey},${lucius_dark_grey}
        face LineNumberCursor ${lucius_grey},${lucius_dark_grey}+b
        face MenuForeground ${lucius_blue},${lucius_dark_blue}
        face MenuBackground ${lucius_darker_grey},${lucius_light_grey}
        face MenuInfo ${lucius_grey}
        face Information ${lucius_lighter_grey},${lucius_dark_green}
        face Error ${lucius_light_red},${lucius_dark_red}
        face StatusLine ${lucius_lighter_grey},${lucius_dark_grey}
        face StatusLineMode ${lucius_lighter_grey},${lucius_dark_green}+b
        face StatusLineInfo ${lucius_dark_grey},${lucius_lighter_grey}
        face StatusLineValue ${lucius_lighter_grey}
        face StatusCursor default,${lucius_blue}
        face Prompt ${lucius_lighter_grey}
        face MatchingChar ${lucius_lighter_grey},${lucius_bright_green}
        face BufferPadding ${lucius_green},${lucius_darker_grey}
    "
}
