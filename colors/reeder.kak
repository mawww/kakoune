##
## reeder theme
## a light theme inspired after https://github.com/hyspace/st2-reeder-theme
##

evaluate-commands %sh{
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
        face global value         ${orange_light}+b
        face global type          ${orange}
        face global variable      default
        face global module        ${green}
        face global function      default
        face global string        ${green}
        face global keyword       ${brown_dark}
        face global operator      default
        face global attribute     ${green}
        face global comment       ${brown_light}
        face global documentation comment
        face global meta          ${brown_dark}
        face global builtin       default+b

        # and markup
        face global title      ${orange}+b
        face global header     ${orange}+b
        face global mono       ${green_light}
        face global block      ${green}
        face global link       ${orange}
        face global bullet     ${brown_dark}
        face global list       ${black}

        # and built in faces
        face global Default            ${black_light},${white}
        face global PrimarySelection   ${black},${brown_lighter}+fg
        face global SecondarySelection ${black_light},${grey_light}+fg
        face global PrimaryCursor      ${black},${grey_dark}+fg
        face global SecondaryCursor    ${black},${grey_dark}+fg
        face global PrimaryCursorEol   ${black},${brown_dark}+fg
        face global SecondaryCursorEol ${black},${brown_dark}+fg
        face global LineNumbers        ${grey_dark},${white}
        face global LineNumberCursor   ${grey_dark},${brown_lighter}
        face global MenuForeground     ${orange},${brown_lighter}
        face global MenuBackground     ${black_light},${brown_lighter}
        face global MenuInfo           default,${black}
        face global Information        ${black_light},${brown_lighter}
        face global Error              default,${red}
        face global DiagnosticError    ${red}
        face global DiagnosticWarning  ${orange}
        face global StatusLine         ${black},${grey_light}
        face global StatusLineMode     ${orange}
        face global StatusLineInfo     ${black}+b
        face global StatusLineValue    ${green_light}
        face global StatusCursor       ${orange},${white_light}
        face global Prompt             ${black_light}
        face global MatchingChar       default+b
        face global BufferPadding      ${grey_dark},${white}
        face global Whitespace         ${grey_dark}+f
    "
}
