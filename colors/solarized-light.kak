# Solarized Light

evaluate-commands %sh{
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
        face global value              ${cyan}
        face global type               ${red}
        face global variable           ${blue}
        face global module             ${cyan}
        face global function           ${blue}
        face global string             ${cyan}
        face global keyword            ${green}
        face global operator           ${yellow}
        face global attribute          ${violet}
        face global comment            ${base1}
        face global documentation      comment
        face global meta               ${orange}
        face global builtin            default+b

        # markup
        face global title              ${blue}+b
        face global header             ${blue}
        face global mono               ${base1}
        face global block              ${cyan}
        face global link               ${base01}
        face global bullet             ${yellow}
        face global list               ${green}

        # builtin
        face global Default            ${base00},${base3}
        face global PrimarySelection   ${base3},${blue}+fg
        face global SecondarySelection ${base1},${base01}+fg
        face global PrimaryCursor      ${base3},${base00}+fg
        face global SecondaryCursor    ${base3},${base1}+fg
        face global PrimaryCursorEol   ${base3},${yellow}+fg
        face global SecondaryCursorEol ${base3},${orange}+fg
        face global LineNumbers        ${base1},${base2}
        face global LineNumberCursor   ${base01},${base2}
        face global LineNumbersWrapped ${base2},${base2}
        face global MenuForeground     ${base3},${yellow}
        face global MenuBackground     ${base01},${base2}
        face global MenuInfo           ${base1}
        face global Information        ${base2},${base1}
        face global Error              ${red},default+b
        face global DiagnosticError    ${red}
        face global DiagnosticWarning  ${yellow}
        face global StatusLine         ${base01},${base2}+b
        face global StatusLineMode     ${orange}
        face global StatusLineInfo     ${cyan}
        face global StatusLineValue    ${green}
        face global StatusCursor       ${base0},${base03}
        face global Prompt             ${yellow}+b
        face global MatchingChar       ${red},${base2}+b
        face global BufferPadding      ${base1},${base3}
        face global Whitespace         ${base1}+f
    "
}
