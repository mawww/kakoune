##
## reeder theme
## a light theme inspired after https://github.com/hyspace/st2-reeder-theme
##

evaluate-commands %sh{
    scope="${1:-global}"

    white="rgb:f9f8f6"
    white_light="rgb:f6f5f0"
    black="rgb:383838"
    black_light="rgb:635240"
    grey_dark="rgb:c6b0a4"
    grey_light="rgb:e8e8e8"
    brown_dark="rgb:af4609"
    brown_light="rgb:baa188"
    brown_lighter="rgb:f0e7df"
    orange="rgb:fc7302"
    orange_light="rgb:f88e3b"
    green="rgb:438047"
    green_light="rgb:7ba84d"
    red="rgb:f03c3c"

    # Base color definitions
    echo "
        # then we map them to code
        face ${scope} value      ${orange_light}+b
        face ${scope} type       ${orange}
        face ${scope} variable   default
        face ${scope} module     ${green}
        face ${scope} function   default
        face ${scope} string     ${green}
        face ${scope} keyword    ${brown_dark}
        face ${scope} operator   default
        face ${scope} attribute  ${green}
        face ${scope} comment    ${brown_light}
        face ${scope} meta       ${brown_dark}
        face ${scope} builtin   default+b

        # and markup
        face ${scope} title      ${orange}+b
        face ${scope} header     ${orange}+b
        face ${scope} bold       default+b
        face ${scope} italic     default+i
        face ${scope} mono       ${green_light}
        face ${scope} block      ${green}
        face ${scope} link       ${orange}
        face ${scope} bullet     ${brown_dark}
        face ${scope} list       ${black}

        # and built in faces
        face ${scope} Default            ${black_light},${white}
        face ${scope} PrimarySelection   ${black},${brown_lighter}+fg
        face ${scope} SecondarySelection ${black_light},${grey_light}+fg
        face ${scope} PrimaryCursor      ${black},${grey_dark}+fg
        face ${scope} SecondaryCursor    ${black},${grey_dark}+fg
        face ${scope} PrimaryCursorEol   ${black},${brown_dark}+fg
        face ${scope} SecondaryCursorEol ${black},${brown_dark}+fg
        face ${scope} LineNumbers        ${grey_dark},${white}
        face ${scope} LineNumberCursor   ${grey_dark},${brown_lighter}
        face ${scope} MenuForeground     ${orange},${brown_lighter}
        face ${scope} MenuBackground     ${black_light},${brown_lighter}
        face ${scope} MenuInfo           default,${black}
        face ${scope} Information        ${black_light},${brown_lighter}
        face ${scope} Error              default,${red}
        face ${scope} StatusLine         ${black},${grey_light}
        face ${scope} StatusLineMode     ${orange}
        face ${scope} StatusLineInfo     ${black}+b
        face ${scope} StatusLineValue    ${green_light}
        face ${scope} StatusCursor       ${orange},${white_light}
        face ${scope} Prompt             ${black_light}
        face ${scope} MatchingChar       default+b
        face ${scope} BufferPadding      ${grey_dark},${white}
        face ${scope} Whitespace         ${grey_dark}+f
    "
}
