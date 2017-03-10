# desertex theme

# Code
face value      rgb:fa8072
face type       rgb:dfdfbf
face identifier rgb:87ceeb
face string     rgb:fa8072
face error      rgb:c3bf9f+b
face keyword    rgb:eedc82
face operator   rgb:87ceeb
face attribute  rgb:eedc82
face comment    rgb:7ccd7c+i

# #include <...>
face meta rgb:ee799f

# Markup
face title  blue
face header cyan
face bold   red
face italic yellow
face mono   green
face block  magenta
face link   cyan
face bullet cyan
face list   yellow

# Builtin
# fg,bg+attributes
# face Default default,rgb:262626 <- change the terminal bg color instead
face Default default,default

face PrimarySelection   white,blue
face SecondarySelection black,blue

face PrimaryCursor   black,white
face SecondaryCursor black,white

face LineNumbers      rgb:605958
face LineNumberCursor yellow,default+b

# Bottom menu:
# text + background
face MenuBackground black,rgb:c2bfa5+b
# selected entry in the menu (use 302028 when true color support is fixed)
face MenuForeground rgb:f0a0c0,magenta

# completion menu info
face MenuInfo white,rgb:445599

# assistant, [+]
face Information black,yellow

face Error      white,red
face StatusLine cyan,default

# Status line modes and prompts:
# insert, prompt, enter key...
face StatusLineMode rgb:ffd75f,default

# 1 sel
face StatusLineInfo blue,default

# param=value, reg=value. ex: "ey
face StatusLineValue green,default

face StatusCursor black,cyan

# :
face Prompt blue

# (), {}
face MatchingChar cyan+b

# EOF tildas (~)
face BufferPadding blue,default
