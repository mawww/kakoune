#
## Tomorrow-night, adapted by nicholastmosher
##

evaluate-commands %sh{
    foreground="rgb:c5c8c6" # gui05
    background="rgb:1d1f21" # gui00
    selection="rgb:373b41"  # gui02
    window="rgb:282a2e"     # gui01
    comment="rgb:969896"    # gui03
    red="rgb:cc6666"        # gui08
    orange="rgb:de935f"     # gui09
    yellow="rgb:f0c674"     # gui0A
    green="rgb:b5bd68"      # gui0B
    blue="rgb:81a2be"       # gui0D
    aqua="rgb:8abeb7"       # gui0C
    purple="rgb:b294bb"     # gui0E

    ## code
    echo "
        face global value ${orange}
        face global type ${yellow}
        face global variable ${red}
        face global module ${blue}
        face global function ${blue}
        face global string ${green}
        face global keyword ${purple}
        face global operator ${aqua}
        face global attribute ${purple}
        face global comment ${comment}
        face global documentation comment
        face global meta ${purple}
        face global builtin ${yellow}
    "

    ## markup
    echo "
        face global title ${blue}
        face global header ${aqua}
        face global mono ${green}
        face global block ${orange}
        face global link ${blue}
        face global bullet ${red}
        face global list ${red}
    "

    ## builtin
    echo "
        face global Default ${foreground},${background}
        face global PrimarySelection ${foreground},${selection}+fg
        face global SecondarySelection ${foreground},${window}+fg
        face global PrimaryCursor ${background},${foreground}+fg
        face global SecondaryCursor ${background},${aqua}+fg
        face global PrimaryCursorEol ${background},${green}+fg
        face global SecondaryCursorEol ${background},${green}+fg
        face global LineNumbers ${comment},${window}
        face global LineNumberCursor ${yellow},${window}+b
        face global MenuForeground ${window},${foreground}
        face global MenuBackground ${foreground},${window}
        face global MenuInfo ${red}
        face global Information ${foreground},${window}
        face global Error ${foreground},${red}
        face global DiagnosticError ${red}
        face global DiagnosticWarning ${yellow}
        face global StatusLine ${foreground},${selection}
        face global StatusLineMode ${yellow}+b
        face global StatusLineInfo ${aqua}
        face global StatusLineValue ${green}
        face global StatusCursor ${window},${aqua}
        face global Prompt ${background},${aqua}
        face global MatchingChar ${yellow},${background}+b
        face global BufferPadding ${aqua},${background}
        face global Whitespace ${comment}+f
    "
}
