# gruvbox theme

evaluate-commands %sh{
    scope="${1:-global}"

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
        face ${scope} value     ${purple}
        face ${scope} type      ${yellow}
        face ${scope} variable  ${blue}
        face ${scope} module    ${green}
        face ${scope} function  ${fg}
        face ${scope} string    ${green}
        face ${scope} keyword   ${red}
        face ${scope} operator  ${fg}
        face ${scope} attribute ${orange}
        face ${scope} comment   ${gray}+i
        face ${scope} meta      ${aqua}
        face ${scope} builtin   ${fg}+b

        # Markdown highlighting
        face ${scope} title     ${green}+b
        face ${scope} header    ${orange}
        face ${scope} bold      ${fg}+b
        face ${scope} italic    ${fg}+i
        face ${scope} mono      ${fg4}
        face ${scope} block     ${aqua}
        face ${scope} link      ${blue}+u
        face ${scope} bullet    ${yellow}
        face ${scope} list      ${fg}

        face ${scope} Default            ${fg},${bg}
        face ${scope} PrimarySelection   ${fg},${blue}+fg
        face ${scope} SecondarySelection ${bg},${blue}+fg
        face ${scope} PrimaryCursor      ${bg},${fg}+fg
        face ${scope} SecondaryCursor    ${bg},${bg4}+fg
        face ${scope} PrimaryCursorEol   ${bg},${fg4}+fg
        face ${scope} SecondaryCursorEol ${bg},${bg2}+fg
        face ${scope} LineNumbers        ${bg4}
        face ${scope} LineNumberCursor   ${yellow},${bg1}
        face ${scope} LineNumbersWrapped ${bg1}
        face ${scope} MenuForeground     ${bg2},${blue}
        face ${scope} MenuBackground     ${fg},${bg2}
        face ${scope} MenuInfo           ${bg}
        face ${scope} Information        ${bg},${fg}
        face ${scope} Error              ${bg},${red}
        face ${scope} StatusLine         ${fg},${bg}
        face ${scope} StatusLineMode     ${yellow}+b
        face ${scope} StatusLineInfo     ${purple}
        face ${scope} StatusLineValue    ${red}
        face ${scope} StatusCursor       ${bg},${fg}
        face ${scope} Prompt             ${yellow}
        face ${scope} MatchingChar       ${fg},${bg3}+b
        face ${scope} BufferPadding      ${bg2},${bg}
        face ${scope} Whitespace         ${bg2}+f
    "
}
