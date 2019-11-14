# Solarized Light

evaluate-commands %sh{
	scope="${1:-global}"

	base03='rgb:002b36'
	base02='rgb:073642'
	base01='rgb:586e75'
	base00='rgb:657b83'
	base0='rgb:839496'
	base1='rgb:93a1a1'
	base2='rgb:eee8d5'
	base3='rgb:fdf6e3'
	yellow='rgb:b58900'
	orange='rgb:cb4b16'
	red='rgb:dc322f'
	magenta='rgb:d33682'
	violet='rgb:6c71c4'
	blue='rgb:268bd2'
	cyan='rgb:2aa198'
	green='rgb:859900'

    echo "
        # code
        face ${scope} value              ${cyan}
        face ${scope} type               ${red}
        face ${scope} variable           ${blue}
        face ${scope} module             ${cyan}
        face ${scope} function           ${blue}
        face ${scope} string             ${cyan}
        face ${scope} keyword            ${green}
        face ${scope} operator           ${yellow}
        face ${scope} attribute          ${violet}
        face ${scope} comment            ${base1}
        face ${scope} meta               ${orange}
        face ${scope} builtin            default+b

        # markup
        face ${scope} title              ${blue}+b
        face ${scope} header             ${blue}
        face ${scope} bold               ${base01}+b
        face ${scope} italic             ${base01}+i
        face ${scope} mono               ${base1}
        face ${scope} block              ${cyan}
        face ${scope} link               ${base01}
        face ${scope} bullet             ${yellow}
        face ${scope} list               ${green}

        # builtin
        face ${scope} Default            ${base00},${base3}
        face ${scope} PrimarySelection   ${base3},${blue}+fg
        face ${scope} SecondarySelection ${base1},${base01}+fg
        face ${scope} PrimaryCursor      ${base3},${base00}+fg
        face ${scope} SecondaryCursor    ${base3},${base1}+fg
        face ${scope} PrimaryCursorEol   ${base3},${yellow}+fg
        face ${scope} SecondaryCursorEol ${base3},${orange}+fg
        face ${scope} LineNumbers        ${base1},${base2}
        face ${scope} LineNumberCursor   ${base01},${base2}
        face ${scope} LineNumbersWrapped ${base2},${base2}
        face ${scope} MenuForeground     ${base3},${yellow}
        face ${scope} MenuBackground     ${base01},${base2}
        face ${scope} MenuInfo           ${base1}
        face ${scope} Information        ${base2},${base1}
        face ${scope} Error              ${red},default+b
        face ${scope} StatusLine         ${base01},${base2}+b
        face ${scope} StatusLineMode     ${orange}
        face ${scope} StatusLineInfo     ${cyan}
        face ${scope} StatusLineValue    ${green}
        face ${scope} StatusCursor       ${base0},${base03}
        face ${scope} Prompt             ${yellow}+b
        face ${scope} MatchingChar       ${red},${base2}+b
        face ${scope} BufferPadding      ${base1},${base3}
        face ${scope} Whitespace         ${base1}+f
    "
}
