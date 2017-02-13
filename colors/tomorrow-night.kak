##
## Tomorrow-night, adapted by nicholastmosher
##

%sh{
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
        face value ${orange}
        face type ${yellow}
        face variable ${magenta}
        face module ${green}
        face function ${aqua}
        face string ${green_dark}
        face keyword ${purple}
        face operator ${aqua}
        face attribute ${purple}
        face comment ${comment}
        face meta ${purple}
        face builtin ${orange}
    "

    ## markup
    echo "
        face title blue
        face header ${aqua}
        face bold ${yellow}
        face italic ${orange}
        face mono ${green_dark}
        face block ${orange}
        face link blue
        face bullet ${red}
        face list ${red}
    "

    ## builtin
    echo "
        face Default ${text},${background}
        face PrimarySelection default,${selection}
        face SecondarySelection default,${selection}
        face PrimaryCursor black,${aqua}
        face SecondaryCursor black,${aqua}
        face LineNumbers ${text_light},${background}
        face LineNumberCursor ${yellow},rgb:282828+b
        face MenuForeground ${text_light},blue
        face MenuBackground ${aqua},${window}
        face MenuInfo ${aqua}
        face Information white,${window}
        face Error white,${red}
        face StatusLine ${text},${window}
        face StatusLineMode ${yellow}+b
        face StatusLineInfo ${aqua}
        face StatusLineValue ${green_dark}
        face StatusCursor ${window},${aqua}
        face Prompt ${background},${aqua}
        face MatchingChar ${yellow},${background}+b
        face BufferPadding ${aqua},${background}
    "
}
