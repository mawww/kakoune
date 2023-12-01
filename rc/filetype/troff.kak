# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.\d+ %{
    set-option buffer filetype troff
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=troff %{
    require-module troff
}

hook -group troff-highlight global WinSetOption filetype=troff %{
    add-highlighter window/troff ref troff
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/troff }
}

provide-module troff %{

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/troff regions

add-highlighter shared/troff/text default-region group
add-highlighter shared/troff/text/ regex '(^\.)?\\".*?\n' 0:comment
add-highlighter shared/troff/text/ regex '\\f[A-Z]' 0:attribute
add-highlighter shared/troff/text/ regex '\\fB(.+?)\\f[A-Z]' 1:+b
add-highlighter shared/troff/text/ regex '\\fI(.+?)\\f[A-Z]' 1:+i
add-highlighter shared/troff/text/ regex '^\.[a-zA-Z]{1,2}\b' 0:meta
add-highlighter shared/troff/text/ regex '^\.(PSPIC|PDFPIC|pdfhref)\b' 0:meta
add-highlighter shared/troff/text/ regex '^\.\.$' 0:meta
add-highlighter shared/troff/text/ regex '^\.TH\s+[^\n]+' 0:title
add-highlighter shared/troff/text/ regex '^\.NH(\s+\d+(\s+\d+)?)?\s*\n' 0:header
add-highlighter shared/troff/text/ regex '^\.SH(\s+\d+)?\s*\n' 0:header
add-highlighter shared/troff/text/ regex '^\.IR\s+(\S+)' 1:+i
add-highlighter shared/troff/text/ regex '^\.BR\s+(\S+)' 1:+b
add-highlighter shared/troff/text/ regex '^\.I\s+([^\n]+)' 1:+i
add-highlighter shared/troff/text/ regex '^\.B\s+([^\n]+)' 1:+b
add-highlighter shared/troff/text/ regex '(ftp:|http:|https:|www\.)+[^\s]+[\w]' 0:link

add-highlighter shared/troff/pic region '^\.PS\b' '^\.PE\b' group
add-highlighter shared/troff/pic/ regex '^(\.PS\b|\.PE\b)' 1:meta
add-highlighter shared/troff/pic/ regex '^(copy)\s+' 1:keyword
add-highlighter shared/troff/pic/ regex '\b(arc|arrow|box|circle|ellipse|line|move|spline)\b' 1:type
add-highlighter shared/troff/pic/ regex '\b(above|at|below|by|center|chop|dashed|diam|diameter|down|dotted|fill|from|ht|height|invis|left|ljust|rad|radius|right|rjust|solid|then|to|up|wid|width|with)\b' 1:attribute
add-highlighter shared/troff/pic/ regex '(\s+|\+|-|\*|/)(\d+(\.\d+)?)' 2:value
add-highlighter shared/troff/pic/ regex '"[^"]*"' 0:string
}
