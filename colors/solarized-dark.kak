# Solarized Dark

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
        face comment            ${base01}
        face meta               ${orange}
        face builtin            default+b

        # markup
        face title              ${blue}+b
        face header             ${blue}
        face bold               ${base0}+b
        face italic             ${base0}+i
        face mono               ${base1}
        face block              ${cyan}
        face link               ${base1}
        face bullet             ${yellow}
        face list               ${green}

        # builtin
        face Default            ${base0},${base03}
        face PrimarySelection   ${base03},${blue}
        face SecondarySelection ${base01},${base1}
        face PrimaryCursor      ${base03},${base0}
        face SecondaryCursor    ${base03},${base01}
        face LineNumbers        ${base01},${base02}
        face LineNumberCursor   ${base1},${base02}
        face LineNumbersWrapped ${base02},${base02}
        face MenuForeground     ${base03},${yellow}
        face MenuBackground     ${base1},${base02}
        face MenuInfo           ${base01}
        face Information        ${base02},${base1}
        face Error              ${red},default+b
        face StatusLine         ${base1},${base02}+b
        face StatusLineMode     ${orange}
        face StatusLineInfo     ${cyan}
        face StatusLineValue    ${green}
        face StatusCursor       ${base00},${base3}
        face Prompt             ${yellow}+b
        face MatchingChar       ${red},${base01}+b
        face BufferPadding      ${base01},${base03}
    "
}
