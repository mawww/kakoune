# Solarized Light

%sh{
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
        face value              ${cyan}
        face type               ${yellow}
        face variable           ${blue}
        face module             ${cyan}
        face function           ${blue}
        face string             ${cyan}
        face keyword            ${green}
        face operator           ${green}
        face attribute          ${violet}
        face comment            ${base1}
        face meta               ${orange}
        face builtin            default+b

        # markup
        face title              ${blue}+b
        face header             ${blue}
        face bold               ${base01}+b
        face italic             ${base01}+i
        face mono               ${base1}
        face block              ${cyan}
        face link               ${base01}
        face bullet             ${yellow}
        face list               ${green}

        # builtin
        face Default            ${base00},${base3}
        face PrimarySelection   ${base3},${blue}
        face SecondarySelection ${base1},${base01}
        face PrimaryCursor      ${base3},${base00}
        face SecondaryCursor    ${base3},${base1}
        face LineNumbers        ${base1},${base2}
        face LineNumberCursor   ${base01},${base2}
        face LineNumbersWrapped ${base2},${base2}
        face MenuForeground     ${base3},${yellow}
        face MenuBackground     ${base01},${base2}
        face MenuInfo           ${base1}
        face Information        ${base2},${base1}
        face Error              ${red},default+b
        face StatusLine         ${base01},${base2}+b
        face StatusLineMode     ${orange}
        face StatusLineInfo     ${cyan}
        face StatusLineValue    ${green}
        face StatusCursor       ${base0},${base03}
        face Prompt             ${yellow}+b
        face MatchingChar       ${red},${base2}+b
        face BufferPadding      ${base1},${base3}
    "
}
