# Solarized Dark

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
        face ${scope} comment            ${base01}
        face ${scope} meta               ${orange}
        face ${scope} builtin            default+b

        # markup
        face ${scope} title              ${blue}+b
        face ${scope} header             ${blue}
        face ${scope} bold               ${base0}+b
        face ${scope} italic             ${base0}+i
        face ${scope} mono               ${base1}
        face ${scope} block              ${cyan}
        face ${scope} link               ${base1}
        face ${scope} bullet             ${yellow}
        face ${scope} list               ${green}

        # builtin
        face ${scope} Default            ${base0},${base03}
        face ${scope} PrimarySelection   ${base03},${blue}+fg
        face ${scope} SecondarySelection ${base01},${base1}+fg
        face ${scope} PrimaryCursor      ${base03},${base0}+fg
        face ${scope} SecondaryCursor    ${base03},${base01}+fg
        face ${scope} PrimaryCursorEol   ${base03},${base2}+fg
        face ${scope} SecondaryCursorEol ${base03},${base3}+fg
        face ${scope} LineNumbers        ${base01},${base02}
        face ${scope} LineNumberCursor   ${base1},${base02}
        face ${scope} LineNumbersWrapped ${base02},${base02}
        face ${scope} MenuForeground     ${base03},${yellow}
        face ${scope} MenuBackground     ${base1},${base02}
        face ${scope} MenuInfo           ${base01}
        face ${scope} Information        ${base02},${base1}
        face ${scope} Error              ${red},default+b
        face ${scope} StatusLine         ${base1},${base02}+b
        face ${scope} StatusLineMode     ${orange}
        face ${scope} StatusLineInfo     ${cyan}
        face ${scope} StatusLineValue    ${green}
        face ${scope} StatusCursor       ${base00},${base3}
        face ${scope} Prompt             ${yellow}+b
        face ${scope} MatchingChar       ${red},${base01}+b
        face ${scope} BufferPadding      ${base01},${base03}
        face ${scope} Whitespace         ${base01}+f
    "
}
