# https://twig.symfony.com/doc/3.x/templates.html
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](twig) %{
    set-option buffer filetype twig
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=twig %[
    require-module twig

    hook window ModeChange pop:insert:.* -group twig-trim-indent twig-trim-indent
    hook window InsertChar \n -group twig-insert twig-insert-on-new-line
    hook window InsertChar \n -group twig-indent twig-indent-on-new-line
    hook window InsertChar '>' -group twig-indent twig-indent-on-greater-than
    hook window InsertChar '#' -group twig-auto-close twig-auto-close-delim
    hook window InsertChar '%' -group twig-auto-close twig-auto-close-delim
    set-option buffer extra_word_chars '_' '-'

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window twig-.+ }
]

hook -group twig-highlight global WinSetOption filetype=twig %{
    add-highlighter window/twig ref twig
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/twig }
}


provide-module twig %[

require-module html

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/twig regions
add-highlighter shared/twig/core default-region group
add-highlighter shared/twig/comment region \{# [#]\} fill comment
add-highlighter shared/twig/delim region \{([%]-?|\{) (-?[%]|\})\} regions

add-highlighter shared/twig/core/ ref html

add-highlighter shared/twig/delim/base default-region group
add-highlighter shared/twig/delim/double_string region '"'  (?<!\\)(\\\\)*" fill string
add-highlighter shared/twig/delim/single_string region "'"  (?<!\\)(\\\\)*' fill string
add-highlighter shared/twig/delim/base/ regex (\w+)\h= 1:variable

add-highlighter shared/twig/delim/base/functions regex \b(\w+)\( 1:function

add-highlighter shared/twig/delim/base/filters regex \b(abs|batch|capitalize|column|convert_encoding|country_name|currency_name|currency_symbol|data_uri|date|date_modify|default|e|escape|filter|first|format|format_currency|format_date|format_datetime|format_number|format_time|html_to_markdown|inline_css|inky_to_html|join|json_encode|keys|language_name|last|length|locale_name|lower|map|markdown_to_html|merge|nl2br|number_format|raw|reduce|replace|reverse|round|slice|slug|sort|spaceless|split|striptags|timezone_name|title|trim|u|upper|url_encode)(\()?\b 1:operator

add-highlighter shared/twig/delim/base/tags regex \b((extends|deprecated|do|flush|import|from|elseif|else|include|set|use)|(end)?(apply|autoescape|block|cache|embed|for|if|macro|sandbox|set|verbatim|with))\b 0:keyword 0:+i


add-highlighter shared/twig/delim/base/delimiter_code regex (\{[%]|[%]\}) 0:function
add-highlighter shared/twig/delim/base/delimiter_output regex (\{\{|\}\}) 0:operator

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden twig-trim-indent                   html-trim-indent
define-command -hidden twig-indent-on-new-line            html-indent-on-new-line
define-command -hidden twig-indent-on-greater-than        html-indent-on-greater-than

define-command -hidden twig-auto-close-delim %[
  evaluate-commands -itersel %[
    try %[
      execute-keys -draft <semicolon>hH<a-k>\h*\{<ret>lyp
      execute-keys <esc>hi<space><esc>hi<space>
    ]
  ]
]

define-command -hidden twig-insert-on-new-line %[
  evaluate-commands -draft -itersel %/
    execute-keys <semicolon>
    try %[
      execute-keys -draft kx<a-k>^\h*\{\[%#\{\]\h+$<ret>
      execute-keys -draft jghd
    ]
  /
]

]
