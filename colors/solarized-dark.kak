# Solarized Dark

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
        face global type               ${yellow}
        face global variable           ${blue}
        face global module             ${cyan}
        face global function           ${blue}
        face global string             ${cyan}
        face global keyword            ${green}
        face global operator           ${green}
        face global attribute          ${violet}
        face global comment            ${base01}
        face global documentation      comment
        face global meta               ${orange}
        face global builtin            default+b

        # markup
        face global title              ${blue}+b
        face global header             ${blue}
        face global mono               ${base1}
        face global block              ${cyan}
        face global link               ${base1}
        face global bullet             ${yellow}
        face global list               ${green}

        # builtin
        face global Default            ${base0},${base03}
        face global PrimarySelection   ${base03},${blue}+fg
        face global SecondarySelection ${base01},${base1}+fg
        face global PrimaryCursor      ${base03},${base0}+fg
        face global SecondaryCursor    ${base03},${base01}+fg
        face global PrimaryCursorEol   ${base03},${base2}+fg
        face global SecondaryCursorEol ${base03},${base3}+fg
        face global LineNumbers        ${base01},${base02}
        face global LineNumberCursor   ${base1},${base02}
        face global LineNumbersWrapped ${base02},${base02}
        face global MenuForeground     ${base03},${yellow}
        face global MenuBackground     ${base1},${base02}
        face global MenuInfo           ${base01}
        face global Information        ${base02},${base1}
        face global Error              ${red},default+b
        face global DiagnosticError    ${red}
        face global DiagnosticWarning  ${yellow}
        face global StatusLine         ${base1},${base02}+b
        face global StatusLineMode     ${orange}
        face global StatusLineInfo     ${cyan}
        face global StatusLineValue    ${green}
        face global StatusCursor       ${base00},${base3}
        face global Prompt             ${yellow}+b
        face global MatchingChar       ${red},${base01}+b
        face global BufferPadding      ${base01},${base03}
        face global Whitespace         ${base01}+f
    "
}
