# gruvbox theme

evaluate-commands %sh{
    gray="rgb:928374"
    red="rgb:fb4934"
    green="rgb:b8bb26"
    yellow="rgb:fabd2f"
    blue="rgb:83a598"
    purple="rgb:d3869b"
    aqua="rgb:8ec07c"
    orange="rgb:fe8019"

    bg="rgb:282828"
    bg1="rgb:3c3836"
    bg2="rgb:504945"
    bg3="rgb:665c54"
    bg4="rgb:7c6f64"

    fg0="rgb:fbf1c7"
    fg="rgb:ebdbb2"
    fg2="rgb:d5c4a1"
    fg3="rgb:bdae93"
    fg4="rgb:a89984"

    echo "
        # Code highlighting
        face global value     ${purple}
        face global type      ${yellow}
        face global variable  ${blue}
        face global module    ${green}
        face global function  default
        face global string    ${green}
        face global keyword   ${red}
        face global operator  default
        face global attribute ${orange}
        face global comment   ${gray}
        face global meta      ${aqua}
        face global builtin   default+b

        # Markdown highlighting
        face global title     ${green}+b
        face global header    ${orange}
        face global bold      ${fg}+b
        face global italic    ${fg3}
        face global mono      ${fg4}
        face global block     default
        face global link      default
        face global bullet    default
        face global list      default

        face global Default            ${fg},${bg}
        face global PrimarySelection   ${fg},${blue}
        face global SecondarySelection ${bg},${blue}
        face global PrimaryCursor      ${bg},${fg}
        face global SecondaryCursor    ${bg},${fg}
        face global PrimaryCursorEol   ${bg},${fg4}
        face global SecondaryCursorEol ${bg},${fg4}
        face global LineNumbers        ${bg4}
        face global LineNumberCursor   ${yellow},${bg1}
        face global LineNumbersWrapped ${bg1}
        face global MenuForeground     ${bg2},${blue}
        face global MenuBackground     default,${bg2}
        face global MenuInfo           ${bg}
        face global Information        ${bg},${fg}
        face global Error              ${bg},${red}
        face global StatusLine         default
        face global StatusLineMode     ${yellow}+b
        face global StatusLineInfo     ${purple}
        face global StatusLineValue    ${red}
        face global StatusCursor       ${bg},${fg}
        face global Prompt             ${yellow}
        face global MatchingChar       default+b
        face global BufferPadding      ${bg2},${bg}
        face global Whitespace         ${bg2}
    "
}
