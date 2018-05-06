##
## Tomorrow-night, adapted by nicholastmosher
##

evaluate-commands %sh{
    foreground="rgb:c5c8c6"
    background="rgb:272727"
    selection="rgb:373b41"
    window="rgb:383838"
    text="rgb:D8D8D8"
    text_light="rgb:4E4E4E"
    line="rgb:282a2e"
    comment="rgb:969896"
    red="rgb:cc6666"
    orange="rgb:d88860"
    yellow="rgb:f0c674"
    green="rgb:b5bd68"
    green_dark="rgb:a1b56c"
    blue="rgb:81a2be"
    aqua="rgb:87afaf"
    magenta="rgb:ab4642"
    purple="rgb:b294bb"

    ## code
    echo "
        face global value ${orange}
        face global type ${yellow}
        face global variable ${magenta}
        face global module ${green}
        face global function ${aqua}
        face global string ${green_dark}
        face global keyword ${purple}
        face global operator ${aqua}
        face global attribute ${purple}
        face global comment ${comment}
        face global meta ${purple}
        face global builtin ${orange}
    "

    ## markup
    echo "
        face global title blue
        face global header ${aqua}
        face global bold ${yellow}
        face global italic ${orange}
        face global mono ${green_dark}
        face global block ${orange}
        face global link blue
        face global bullet ${red}
        face global list ${red}
    "

    ## builtin
    echo "
        face global Default ${text},${background}
        face global PrimarySelection default,${selection}
        face global SecondarySelection default,${selection}
        face global PrimaryCursor black,${aqua}
        face global SecondaryCursor black,${aqua}
        face global PrimaryCursorEol black,${green_dark}
        face global SecondaryCursorEol black,${green_dark}
        face global LineNumbers ${text_light},${background}
        face global LineNumberCursor ${yellow},rgb:282828+b
        face global MenuForeground ${text_light},blue
        face global MenuBackground ${aqua},${window}
        face global MenuInfo ${aqua}
        face global Information white,${window}
        face global Error white,${red}
        face global StatusLine ${text},${window}
        face global StatusLineMode ${yellow}+b
        face global StatusLineInfo ${aqua}
        face global StatusLineValue ${green_dark}
        face global StatusCursor ${window},${aqua}
        face global Prompt ${background},${aqua}
        face global MatchingChar ${yellow},${background}+b
        face global BufferPadding ${aqua},${background}
    "
}
