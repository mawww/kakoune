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

add-highlighter shared/troff group

add-highlighter shared/troff/ regex '(^\.)?\\".*?\n' 0:comment

add-highlighter shared/troff/ regex '\\f[A-Z]' 0:attribute
add-highlighter shared/troff/ regex '\\fB(.+?)\\f[A-Z]' 1:+b
add-highlighter shared/troff/ regex '\\fI(.+?)\\f[A-Z]' 1:+i

add-highlighter shared/troff/ regex '^\.[a-zA-Z]{1,2}\b' 0:meta
add-highlighter shared/troff/ regex '^\.\.$' 0:meta
add-highlighter shared/troff/ regex '^\.TH\s+[^\n]+' 0:title
add-highlighter shared/troff/ regex '^\.SH\s+[^\n]+' 0:header
add-highlighter shared/troff/ regex '^\.IR\s+(\S+)' 1:+i
add-highlighter shared/troff/ regex '^\.BR\s+(\S+)' 1:+b
add-highlighter shared/troff/ regex '^\.I\s+([^\n]+)' 1:+i
add-highlighter shared/troff/ regex '^\.B\s+([^\n]+)' 1:+b

}
