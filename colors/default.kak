# Kakoune default color scheme

evaluate-commands %sh{
    scope="${1:-global}"

    cat <<- EOF

    # For Code
    set-face "${scope}" value red
    set-face "${scope}" type yellow
    set-face "${scope}" variable green
    set-face "${scope}" module green
    set-face "${scope}" function cyan
    set-face "${scope}" string magenta
    set-face "${scope}" keyword blue
    set-face "${scope}" operator yellow
    set-face "${scope}" attribute green
    set-face "${scope}" comment cyan
    set-face "${scope}" meta magenta
    set-face "${scope}" builtin default+b

    # For markup
    set-face "${scope}" title blue
    set-face "${scope}" header cyan
    set-face "${scope}" bold red
    set-face "${scope}" italic yellow
    set-face "${scope}" mono green
    set-face "${scope}" block magenta
    set-face "${scope}" link cyan
    set-face "${scope}" bullet cyan
    set-face "${scope}" list yellow

    # builtin faces
    set-face "${scope}" Default default,default
    set-face "${scope}" PrimarySelection white,blue+fg
    set-face "${scope}" SecondarySelection black,blue+fg
    set-face "${scope}" PrimaryCursor black,white+fg
    set-face "${scope}" SecondaryCursor black,white+fg
    set-face "${scope}" PrimaryCursorEol black,cyan+fg
    set-face "${scope}" SecondaryCursorEol black,cyan+fg
    set-face "${scope}" LineNumbers default,default
    set-face "${scope}" LineNumberCursor default,default+r
    set-face "${scope}" MenuForeground white,blue
    set-face "${scope}" MenuBackground blue,white
    set-face "${scope}" MenuInfo cyan
    set-face "${scope}" Information black,yellow
    set-face "${scope}" Error black,red
    set-face "${scope}" StatusLine cyan,default
    set-face "${scope}" StatusLineMode yellow,default
    set-face "${scope}" StatusLineInfo blue,default
    set-face "${scope}" StatusLineValue green,default
    set-face "${scope}" StatusCursor black,cyan
    set-face "${scope}" Prompt yellow,default
    set-face "${scope}" MatchingChar default,default+b
    set-face "${scope}" Whitespace default,default+f
    set-face "${scope}" BufferPadding blue,default

EOF
}
