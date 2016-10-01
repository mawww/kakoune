# http://sass-lang.com
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require css.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-scss %{
    set buffer filetype scss
}

hook global BufCreate .*[.](scss) %{
    set buffer filetype scss
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / group scss

addhl -group /scss ref css

addhl -group /scss regex @[A-Za-z][A-Za-z0-9_-]* 0:meta

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _scss_filter_around_selections      _css_filter_around_selections
def -hidden _scss_indent_on_new_line            _css_indent_on_new_line
def -hidden _scss_indent_on_closing_curly_brace _css_indent_on_closing_curly_brace

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group scss-highlight global WinSetOption filetype=scss %{ addhl ref scss }

hook global WinSetOption filetype=scss %[
    hook window InsertEnd  .* -group scss-hooks  _scss_filter_around_selections
    hook window InsertChar \n -group scss-indent _scss_indent_on_new_line
    hook window InsertChar \} -group scss-indent _scss_indent_on_closing_curly_brace
]

hool -group scss-highlight global WinSetOption filetype=(?!scss).* %{ rmhl scss }

hook global WinSetOption filetype=(?!scss).* %{
    rmhooks window scss-indent
    rmhooks window scss-hooks
}
