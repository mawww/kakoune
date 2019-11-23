##
## github.kak by lenormf
## v1.0
##

evaluate-commands %sh{
    scope="${1:-global}"

    cat <<- EOF

    ## code
    set-face "${scope}" value rgb:0086B3+b
    set-face "${scope}" type rgb:795DA3
    set-face "${scope}" variable rgb:0086B3
    set-face "${scope}" module rgb:0086B3
    set-face "${scope}" function rgb:A71D5D
    set-face "${scope}" string rgb:183691
    set-face "${scope}" keyword rgb:A71D5D+b
    set-face "${scope}" operator yellow
    set-face "${scope}" attribute rgb:A71D5D
    set-face "${scope}" comment rgb:AAAAAA
    set-face "${scope}" meta rgb:183691
    set-face "${scope}" builtin default+b

    ## markup
    set-face "${scope}" title blue
    set-face "${scope}" header cyan
    set-face "${scope}" bold red
    set-face "${scope}" italic yellow
    set-face "${scope}" mono green
    set-face "${scope}" block magenta
    set-face "${scope}" link cyan
    set-face "${scope}" bullet cyan
    set-face "${scope}" list yellow

    ## builtin
    set-face "${scope}" Default rgb:121213,rgb:F8F8FF
    set-face "${scope}" PrimarySelection default,rgb:A6F3A6+fg
    set-face "${scope}" SecondarySelection default,rgb:DBFFDB+fg
    set-face "${scope}" PrimaryCursor black,rgb:888888+fg
    set-face "${scope}" SecondaryCursor black,rgb:888888+fg
    set-face "${scope}" PrimaryCursorEol black,rgb:A71D5D+fg
    set-face "${scope}" SecondaryCursorEol black,rgb:A71D5D+fg
    set-face "${scope}" LineNumbers rgb:A0A0A0,rgb:ECECEC
    set-face "${scope}" LineNumberCursor rgb:434343,rgb:DDDDDD
    set-face "${scope}" MenuForeground rgb:434343,rgb:CDCDFD
    set-face "${scope}" MenuBackground rgb:F8F8FF,rgb:808080
    set-face "${scope}" Information rgb:F8F8FF,rgb:4078C0
    set-face "${scope}" Error rgb:F8F8FF,rgb:BD2C00
    set-face "${scope}" StatusLine rgb:434343,rgb:DDDDDD
    set-face "${scope}" StatusCursor rgb:434343,rgb:CDCDFD
    set-face "${scope}" Prompt rgb:F8F8FF,rgb:4078C0
    set-face "${scope}" MatchingChar rgb:F8F8FF,rgb:4078C0+b
    set-face "${scope}" Search default,default+u
    set-face "${scope}" BufferPadding rgb:A0A0A0,rgb:F8F8FF
    set-face "${scope}" Whitespace rgb:A0A0A0+f

EOF
}
