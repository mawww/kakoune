# Kakoune default color scheme
#
# Template: "%val{runtime}/colors/default.kak"
# Documentation: "%val{runtime}/doc/faces.asciidoc"

# For code
set-face global value red
set-face global type yellow
set-face global variable green
set-face global module green
set-face global function cyan
set-face global string magenta
set-face global keyword blue
set-face global operator yellow
set-face global attribute green
set-face global comment cyan
set-face global documentation comment
set-face global meta magenta
set-face global builtin default+b

# For markup
set-face global title blue
set-face global header cyan
set-face global mono green
set-face global block magenta
set-face global link cyan
set-face global bullet cyan
set-face global list yellow

# Builtin faces
set-face global Default default,default
set-face global PrimarySelection white,blue+fg
set-face global SecondarySelection black,blue+fg
set-face global PrimaryCursor black,white+fg
set-face global SecondaryCursor black,white+fg
set-face global PrimaryCursorEol black,cyan+fg
set-face global SecondaryCursorEol black,cyan+fg
set-face global MenuForeground white,blue
set-face global MenuBackground blue,white
set-face global MenuInfo cyan
set-face global Information black,yellow
set-face global Error black,red
set-face global StatusLine cyan,default
set-face global StatusLineMode yellow,default
set-face global StatusLineInfo blue,default
set-face global StatusLineValue green,default
set-face global StatusCursor black,cyan
set-face global Prompt yellow,default
set-face global BufferPadding blue,default

# Builtin highlighter faces
set-face global LineNumbers default,default
set-face global LineNumberCursor default,default+r
set-face global LineNumbersWrapped default,default+i
set-face global MatchingChar default,default+b
set-face global Whitespace default,default+f
set-face global WrapMarker Whitespace
