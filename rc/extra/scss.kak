# http://sass-lang.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require css.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](scss) %{
    set buffer filetype scss
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / group scss

add-highlighter -group /scss ref css

add-highlighter -group /scss regex @[A-Za-z][A-Za-z0-9_-]* 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _scss_filter_around_selections      _css_filter_around_selections
def -hidden _scss_indent_on_new_line            _css_indent_on_new_line
def -hidden _scss_indent_on_closing_curly_brace _css_indent_on_closing_curly_brace

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group scss-highlight global WinSetOption filetype=scss %{ add-highlighter ref scss }

hook global WinSetOption filetype=scss %[
    hook window InsertEnd  .* -group scss-hooks  _scss_filter_around_selections
    hook window InsertChar \n -group scss-indent _scss_indent_on_new_line
    hook window InsertChar \} -group scss-indent _scss_indent_on_closing_curly_brace
]

hook -group scss-highlight global WinSetOption filetype=(?!scss).* %{ remove-highlighter scss }

hook global WinSetOption filetype=(?!scss).* %{
    remove-hooks window scss-indent
    remove-hooks window scss-hooks
}
