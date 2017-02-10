##
## reeder theme
## a light theme inspired after https://github.com/hyspace/st2-reeder-theme
##

%sh{
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
        face value      ${orange_light}+b
        face type       ${orange}
        face variable   default
        face module     ${green}
        face function   default
        face string     ${green}
        face keyword    ${brown_dark}
        face operator   default
        face attribute  ${green}
        face comment    ${brown_light}
        face meta       ${brown_dark}
        face builtin   default+b

        # and markup
        face title      ${orange}+b
        face header     ${orange}+b
        face bold       default+b
        face italic     default+i
        face mono       ${green_light}
        face block      ${green}
        face link       ${orange}
        face bullet     ${brown_dark}
        face list       ${black}

        # and built in faces
        face Default            ${black_light},${white}
        face PrimarySelection   ${black},${brown_lighter}
        face SecondarySelection ${black_light},${grey_light}
        face PrimaryCursor      ${black},white
        face SecondaryCursor    ${black},white
        face LineNumbers        ${grey_dark},${white}
        face LineNumberCursor   ${grey_dark},${brown_lighter}
        face MenuForeground     ${orange},${brown_lighter}
        face MenuBackground     ${black_light},${brown_lighter}
        face MenuInfo           default,${black}
        face Information        ${black_light},${brown_lighter}
        face Error              default,${red}
        face StatusLine         ${black},${grey_light}
        face StatusLineMode     ${orange}
        face StatusLineInfo     ${black}+b
        face StatusLineValue    ${green_light}
        face StatusCursor       ${orange},${white_light}
        face Prompt             ${black_light}
        face MatchingChar       default+b
        face BufferPadding      ${grey_dark},${white}
    "
}
