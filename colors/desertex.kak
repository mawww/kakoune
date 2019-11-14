# desertex theme

evaluate-commands %sh{
    scope="${1:-global}"

    cat <<- EOF

    # Code
    set-face "${scope}" value      rgb:fa8072
    set-face "${scope}" type       rgb:dfdfbf
    set-face "${scope}" identifier rgb:87ceeb
    set-face "${scope}" string     rgb:fa8072
    set-face "${scope}" error      rgb:c3bf9f+b
    set-face "${scope}" keyword    rgb:eedc82
    set-face "${scope}" operator   rgb:87ceeb
    set-face "${scope}" attribute  rgb:eedc82
    set-face "${scope}" comment    rgb:7ccd7c+i

    # #include <...>
    set-face "${scope}" meta rgb:ee799f

    # Markup
    set-face "${scope}" title  blue
    set-face "${scope}" header cyan
    set-face "${scope}" bold   red
    set-face "${scope}" italic yellow
    set-face "${scope}" mono   green
    set-face "${scope}" block  magenta
    set-face "${scope}" link   cyan
    set-face "${scope}" bullet cyan
    set-face "${scope}" list   yellow

    # Builtin
    # fg,bg+attributes
    set-# face "${scope}" Default default,rgb:262626 <- change the terminal bg color instead
    set-face "${scope}" Default default,default

    set-face "${scope}" PrimarySelection   white,blue+fg
    set-face "${scope}" SecondarySelection black,blue+fg

    set-face "${scope}" PrimaryCursor   black,white+fg
    set-face "${scope}" SecondaryCursor black,white+fg

    set-face "${scope}" PrimaryCursorEol   black,rgb:7ccd7c+fg
    set-face "${scope}" SecondaryCursorEol black,rgb:7ccd7c+fg

    set-face "${scope}" LineNumbers      rgb:605958
    set-face "${scope}" LineNumberCursor yellow,default+b

    # Bottom menu:
    # text + background
    set-face "${scope}" MenuBackground black,rgb:c2bfa5+b
    # selected entry in the menu (use 302028 when true color support is fixed)
    set-face "${scope}" MenuForeground rgb:f0a0c0,magenta

    # completion menu info
    set-face "${scope}" MenuInfo white,rgb:445599

    # assistant, [+]
    set-face "${scope}" Information black,yellow

    set-face "${scope}" Error      white,red
    set-face "${scope}" StatusLine cyan,default

    # Status line modes and prompts:
    # insert, prompt, enter key...
    set-face "${scope}" StatusLineMode rgb:ffd75f,default

    # 1 sel
    set-face "${scope}" StatusLineInfo blue,default

    # param=value, reg=value. ex: "ey
    set-face "${scope}" StatusLineValue green,default

    set-face "${scope}" StatusCursor black,cyan

    # :
    set-face "${scope}" Prompt blue

    # (), {}
    set-face "${scope}" MatchingChar cyan+b

    # EOF tildas (~)
    set-face "${scope}" BufferPadding blue,default

    # Whitespace characters
    set-face "${scope}" Whitespace default+f

EOF
}
