##
## Tomorrow-night, adapted by nicholastmosher
##

evaluate-commands %sh{
    scope="${1:-global}"

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
        face $scope value ${orange}
        face $scope type ${yellow}
        face $scope variable ${magenta}
        face $scope module ${green}
        face $scope function ${aqua}
        face $scope string ${green_dark}
        face $scope keyword ${purple}
        face $scope operator ${aqua}
        face $scope attribute ${purple}
        face $scope comment ${comment}
        face $scope meta ${purple}
        face $scope builtin ${orange}
    "

    ## markup
    echo "
        face $scope title blue
        face $scope header ${aqua}
        face $scope bold ${yellow}
        face $scope italic ${orange}
        face $scope mono ${green_dark}
        face $scope block ${orange}
        face $scope link blue
        face $scope bullet ${red}
        face $scope list ${red}
    "

    ## builtin
    echo "
        face $scope Default ${text},${background}
        face $scope PrimarySelection default,${selection}+fg
        face $scope SecondarySelection default,${selection}+fg
        face $scope PrimaryCursor black,${aqua}+fg
        face $scope SecondaryCursor black,${aqua}+fg
        face $scope PrimaryCursorEol black,${green_dark}+fg
        face $scope SecondaryCursorEol black,${green_dark}+fg
        face $scope LineNumbers ${text_light},${background}
        face $scope LineNumberCursor ${yellow},rgb:282828+b
        face $scope MenuForeground ${text_light},blue
        face $scope MenuBackground ${aqua},${window}
        face $scope MenuInfo ${aqua}
        face $scope Information white,${window}
        face $scope Error white,${red}
        face $scope StatusLine ${text},${window}
        face $scope StatusLineMode ${yellow}+b
        face $scope StatusLineInfo ${aqua}
        face $scope StatusLineValue ${green_dark}
        face $scope StatusCursor ${window},${aqua}
        face $scope Prompt ${background},${aqua}
        face $scope MatchingChar ${yellow},${background}+b
        face $scope BufferPadding ${aqua},${background}
        face $scope Whitespace ${text_light}+f
    "
}
