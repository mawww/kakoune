# Solarized Dark (with termcolors)
# Useful if you've set up your terminal with the exact Solarized colors

evaluate-commands %sh{
    scope="${1:-global}"

    cat <<- EOF

    # code
    set-face "${scope}" value              cyan
    set-face "${scope}" type               yellow
    set-face "${scope}" variable           blue
    set-face "${scope}" module             cyan
    set-face "${scope}" function           blue
    set-face "${scope}" string             cyan
    set-face "${scope}" keyword            green
    set-face "${scope}" operator           green
    set-face "${scope}" attribute          bright-magenta
    set-face "${scope}" comment            bright-green
    set-face "${scope}" meta               bright-red
    set-face "${scope}" builtin            default+b

    # markup
    set-face "${scope}" title              blue+b
    set-face "${scope}" header             blue
    set-face "${scope}" bold               bright-blue+b
    set-face "${scope}" italic             bright-blue+i
    set-face "${scope}" mono               bright-cyan
    set-face "${scope}" block              cyan
    set-face "${scope}" link               bright-cyan
    set-face "${scope}" bullet             yellow
    set-face "${scope}" list               green

    # builtin
    set-face "${scope}" Default            bright-blue,bright-black
    set-face "${scope}" PrimarySelection   bright-black,blue+fg
    set-face "${scope}" SecondarySelection bright-green,bright-cyan+fg
    set-face "${scope}" PrimaryCursor      bright-black,bright-blue+fg
    set-face "${scope}" SecondaryCursor    bright-black,bright-green+fg
    set-face "${scope}" PrimaryCursorEol   bright-black,white+fg
    set-face "${scope}" SecondaryCursorEol bright-black,bright-white+fg
    set-face "${scope}" LineNumbers        bright-green,black
    set-face "${scope}" LineNumberCursor   bright-cyan,black
    set-face "${scope}" LineNumbersWrapped black,black
    set-face "${scope}" MenuForeground     bright-black,yellow
    set-face "${scope}" MenuBackground     bright-cyan,black
    set-face "${scope}" MenuInfo           bright-green
    set-face "${scope}" Information        black,bright-cyan
    set-face "${scope}" Error              red,default+b
    set-face "${scope}" StatusLine         bright-cyan,black+b
    set-face "${scope}" StatusLineMode     bright-red
    set-face "${scope}" StatusLineInfo     cyan
    set-face "${scope}" StatusLineValue    green
    set-face "${scope}" StatusCursor       bright-yellow,bright-white
    set-face "${scope}" Prompt             yellow+b
    set-face "${scope}" MatchingChar       red,bright-green+b
    set-face "${scope}" BufferPadding      bright-green,bright-black
    set-face "${scope}" Whitespace         blue+f

EOF
}
