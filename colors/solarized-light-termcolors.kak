# Solarized Light (with termcolors)
# Useful if you've set up your terminal with the exact Solarized colors

evaluate-commands %sh{
    scope="${1:-global}"

    cat <<- EOF

    # code
    set-face "${scope}" value              cyan
    set-face "${scope}" type               red
    set-face "${scope}" variable           blue
    set-face "${scope}" module             cyan
    set-face "${scope}" function           blue
    set-face "${scope}" string             cyan
    set-face "${scope}" keyword            green
    set-face "${scope}" operator           yellow
    set-face "${scope}" attribute          bright-magenta
    set-face "${scope}" comment            bright-cyan
    set-face "${scope}" meta               bright-red
    set-face "${scope}" builtin            default+b

    # markup
    set-face "${scope}" title              blue+b
    set-face "${scope}" header             blue
    set-face "${scope}" bold               bright-green+b
    set-face "${scope}" italic             bright-green+i
    set-face "${scope}" mono               bright-cyan
    set-face "${scope}" block              cyan
    set-face "${scope}" link               bright-green
    set-face "${scope}" bullet             yellow
    set-face "${scope}" list               green

    # builtin
"${scope}" "${scope}" Default            bright-yellow,bright-white
    set-face "${scope}" PrimarySelection   bright-white,blue+fg
    set-face "${scope}" SecondarySelection bright-cyan,bright-green+fg
    set-face "${scope}" PrimaryCursor      bright-white,bright-yellow+fg
    set-face "${scope}" SecondaryCursor    bright-white,bright-cyan+fg
    set-face "${scope}" PrimaryCursorEol   bright-white,yellow+fg
    set-face "${scope}" SecondaryCursorEol bright-white,bright-red+fg
    set-face "${scope}" LineNumbers        bright-cyan,white
    set-face "${scope}" LineNumberCursor   bright-green,white
    set-face "${scope}" LineNumbersWrapped white,white
    set-face "${scope}" MenuForeground     bright-white,yellow
    set-face "${scope}" MenuBackground     bright-green,white
    set-face "${scope}" MenuInfo           bright-cyan
    set-face "${scope}" Information        white,bright-cyan
    set-face "${scope}" Error              red,default+b
    set-face "${scope}" StatusLine         bright-green,white+b
    set-face "${scope}" StatusLineMode     bright-red
    set-face "${scope}" StatusLineInfo     cyan
    set-face "${scope}" StatusLineValue    green
    set-face "${scope}" StatusCursor       bright-blue,bright-black
    set-face "${scope}" Prompt             yellow+b
    set-face "${scope}" MatchingChar       red,white+b
    set-face "${scope}" BufferPadding      bright-cyan,bright-white
    set-face "${scope}" Whitespace         yellow+f

EOF
}
