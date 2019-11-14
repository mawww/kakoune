# Kakoune simple colors, mostly default

evaluate-commands %sh{
    scope="${1:-global}"

    cat <<- EOF

    # For default
    set-face "${scope}" value default
    set-face "${scope}" type default
    set-face "${scope}" identifier default
    set-face "${scope}" string blue
    set-face "${scope}" keyword default
    set-face "${scope}" operator default
    set-face "${scope}" attribute default
    set-face "${scope}" comment blue
    set-face "${scope}" meta default
    set-face "${scope}" builtin default


    # For default
    set-face "${scope}" title default
    set-face "${scope}" header default
    set-face "${scope}" bold default
    set-face "${scope}" italic default
    set-face "${scope}" mono default
    set-face "${scope}" block default
    set-face "${scope}" link blue
    set-face "${scope}" bullet default
    set-face "${scope}" list default

    # builtin default
    set-face "${scope}" Default default,default
    set-face "${scope}" PrimarySelection white,blue
    set-face "${scope}" SecondarySelection black,blue
    set-face "${scope}" PrimaryCursor black,white
    set-face "${scope}" SecondaryCursor white,blue
    set-face "${scope}" PrimaryCursorEol default
    set-face "${scope}" SecondaryCursorEol default
    set-face "${scope}" LineNumbers default
    set-face "${scope}" LineNumberCursor default
    set-face "${scope}" MenuForeground default
    set-face "${scope}" MenuBackground default
    set-face "${scope}" MenuInfo default
    set-face "${scope}" Information default
    set-face "${scope}" Error default
    set-face "${scope}" StatusLine default
    set-face "${scope}" StatusLineMode default
    set-face "${scope}" StatusLineInfo default
    set-face "${scope}" StatusLineValue default
    set-face "${scope}" StatusCursor default+r
    set-face "${scope}" Prompt default
    set-face "${scope}" MatchingChar default
    set-face "${scope}" BufferPadding default

EOF
}
